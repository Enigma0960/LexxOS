#!/usr/bin/env python3
"""Pack a flat RV32I binary into a LexxOS .lexx executable."""

from __future__ import annotations

import argparse
import datetime
import json
import os
import pathlib
import re
import struct
import sys
import zlib

MAGIC = 0x5858454C  # "LEXX", little-endian
FORMAT_VERSION = 1
ABI_VERSION = 1
MACHINE_RV32I = 1
HEADER_SIZE = 64
IMAGE_OFFSET = HEADER_SIZE
MAX_GUEST_BYTES = 64 * 1024  # Matches the v1 RV32I VM hard limit.
MANIFEST_MAGIC = b"LXMF"
INPUT_EVENTS_FLAG = 1 << 0  # LEXX header.flags: manifest input.mode="events"
MANIFEST_FOOTER = struct.Struct("<4sII")  # magic, JSON byte count, CRC-32
RESOURCE_FOOTER = struct.Struct("<4sIII")  # magic, count, table bytes, table CRC-32
RESOURCE_RECORD = struct.Struct("<IIH")  # absolute data offset, length, alias byte count
# LXRS (legacy) is retained for small applications. Large catalogs use LXR2:
# [records][u32 relative record offsets][LXR2 footer]. A binary lookup only
# seeks into the compact offset table and the selected record.
MAX_RESOURCES = 65535
LEGACY_RESOURCE_LIMIT = 128
MAX_RESOURCE_ALIAS_BYTES = 127
MAX_RESOURCE_TABLE_BYTES = 8 * 1024 * 1024
MAX_MANIFEST_BYTES = 4096
HEADER_STRUCT = struct.Struct("<IHHHHIIIIIIII20s")
DEFAULT_CAPABILITIES = ["console,keyboard"]
CAPABILITIES = {
    "console": 1 << 0,
    "keyboard": 1 << 1,
    "storage-read": 1 << 2,
    "storage-write": 1 << 3,
    "network": 1 << 4,
    "devices": 1 << 5,
    "audio": 1 << 6,
    "graphics": 1 << 7,
    "clock": 1 << 8,
}


def parse_integer(value: str) -> int:
    return int(value, 0)


def parse_capabilities(values: list[str]) -> int:
    result = 0
    for value in values:
        for item in value.split(","):
            name = item.strip()
            if not name:
                continue
            try:
                result |= CAPABILITIES[name]
            except KeyError as exc:
                valid = ", ".join(sorted(CAPABILITIES))
                raise argparse.ArgumentTypeError(f"unknown capability '{name}', valid: {valid}") from exc
    return result


def align_up(value: int, alignment: int) -> int:
    return (value + alignment - 1) & ~(alignment - 1)


def elf_allocated_end(path: pathlib.Path) -> int:
    """Highest end of SHF_ALLOC sections, including NOBITS (.bss), in ELF32/LE."""
    binary = path.read_bytes()
    if len(binary) < 52 or binary[:4] != b"\x7fELF" or binary[4:6] != b"\x01\x01":
        raise ValueError("expected little-endian ELF32")
    if struct.unpack_from("<H", binary, 18)[0] != 243:  # EM_RISCV
        raise ValueError("expected RISC-V ELF")
    section_offset = struct.unpack_from("<I", binary, 32)[0]
    entry_size, section_count = struct.unpack_from("<HH", binary, 46)
    if entry_size < 40 or section_count == 0 or section_offset + entry_size * section_count > len(binary):
        raise ValueError("invalid ELF section table")
    end = 0
    for index in range(section_count):
        section = struct.unpack_from("<IIIIII", binary, section_offset + index * entry_size)
        flags, address, length = section[2], section[3], section[5]
        if flags & 2:  # SHF_ALLOC
            end = max(end, address + length)
    if not end or end > 0xFFFFFFFF:
        raise ValueError("invalid ELF allocated memory range")
    return end


def build_header(
        *,
        capabilities: int,
        entry: int,
        image_size: int,
        memory_size: int,
        stack_size: int,
        bss_size: int,
        flags: int = 0,
) -> bytes:
    if HEADER_STRUCT.size != HEADER_SIZE:
        raise RuntimeError(f"internal header layout error: expected {HEADER_SIZE} bytes, got {HEADER_STRUCT.size}")

    return HEADER_STRUCT.pack(
        MAGIC,
        FORMAT_VERSION,
        HEADER_SIZE,
        MACHINE_RV32I,
        ABI_VERSION,
        flags,
        capabilities,
        entry,
        IMAGE_OFFSET,
        image_size,
        memory_size,
        stack_size,
        bss_size,
        bytes(20),
    )


def build_timestamp() -> str:
    """Record the pack timestamp in UTC, honoring reproducible-build metadata."""
    source_date_epoch = os.environ.get("SOURCE_DATE_EPOCH")
    if source_date_epoch is None:
        instant = datetime.datetime.now(datetime.timezone.utc)
    else:
        try:
            instant = datetime.datetime.fromtimestamp(int(source_date_epoch), tz=datetime.timezone.utc)
        except (ValueError, OverflowError, OSError) as exc:
            raise ValueError("invalid SOURCE_DATE_EPOCH") from exc
    return instant.replace(microsecond=0).isoformat().replace("+00:00", "Z")


def pack_manifest(path: pathlib.Path, *, capabilities: int | None = None,
                   memory_size: int | None = None, stack_size: int | None = None) -> tuple[bytes, int]:
    """Validate the optional build/runtime contract and append metadata."""
    try:
        manifest = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise ValueError(f"cannot read manifest {path}: {exc}") from exc

    if not isinstance(manifest, dict):
        raise ValueError("manifest must be a JSON object")
    if type(manifest.get("schema")) is not int or manifest["schema"] != 1:
        raise ValueError("manifest.schema must equal integer 1")
    for field in ("id", "name", "version", "author", "license"):
        value = manifest.get(field)
        if not isinstance(value, str) or not value.strip():
            raise ValueError(f"manifest.{field} must be a non-empty string")
        if len(value) > 256:
            raise ValueError(f"manifest.{field} is too long")
    if re.fullmatch(r"[a-zA-Z0-9][a-zA-Z0-9._-]*", manifest["id"]) is None:
        raise ValueError("manifest.id must contain only letters, digits, periods, '_' or '-'")
    if re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?", manifest["version"]) is None:
        raise ValueError("manifest.version must be a semantic version (e.g. 1.2.3)")
    if "build" in manifest:
        raise ValueError("manifest.build is generated by the packer and must not be provided")
    if "description" in manifest and not isinstance(manifest["description"], str):
        raise ValueError("manifest.description must be a string")
    if "contacts" in manifest:
        contacts = manifest["contacts"]
        if not isinstance(contacts, dict) or any(not isinstance(v, str) for v in contacts.values()):
            raise ValueError("manifest.contacts must be a JSON object of strings")

    input_mode = "terminal"
    if "input" in manifest:
        settings = manifest["input"]
        if not isinstance(settings, dict) or set(settings) != {"mode"}:
            raise ValueError("manifest.input must be an object containing only mode")
        input_mode = settings["mode"]
        if input_mode not in ("terminal", "events") or not isinstance(input_mode, str):
            raise ValueError("manifest.input.mode must be 'terminal' or 'events'")

    # Legacy manifests without capabilities/memory remain valid. New apps may
    # declare a redundant, human-readable contract; the binary header remains
    # the authoritative runtime permission and allocation record.
    if "capabilities" in manifest:
        names = manifest["capabilities"]
        if not isinstance(names, list) or not names or any(
                not isinstance(name, str) or name not in CAPABILITIES for name in names):
            raise ValueError("manifest.capabilities must be a non-empty array of known names")
        if len(set(names)) != len(names):
            raise ValueError("manifest.capabilities contains duplicates")
        mask = parse_capabilities(names)
        if capabilities is not None and mask != capabilities:
            raise ValueError("manifest.capabilities does not match the executable --cap mask")
    if "memory" in manifest:
        budget = manifest["memory"]
        if not isinstance(budget, dict) or set(budget) != {"guest_bytes", "stack_bytes"}:
            raise ValueError("manifest.memory must contain guest_bytes and stack_bytes")
        if any(type(v) is not int or v <= 0 for v in budget.values()):
            raise ValueError("manifest.memory values must be positive integers")
        if ((memory_size is not None and budget["guest_bytes"] != memory_size)
                or (stack_size is not None and budget["stack_bytes"] != stack_size)):
            raise ValueError("manifest.memory does not match the executable --memory/--stack")

    # Source file paths are build-only metadata and must not leak into the executable.
    manifest.pop("resources", None)

    manifest["build"] = {"timestamp": build_timestamp()}
    payload = (json.dumps(manifest, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    if len(payload) > MAX_MANIFEST_BYTES:
        raise ValueError(f"embedded manifest exceeds {MAX_MANIFEST_BYTES} bytes")
    footer = MANIFEST_FOOTER.pack(MANIFEST_MAGIC, len(payload), zlib.crc32(payload))
    flags = INPUT_EVENTS_FLAG if input_mode == "events" else 0
    return payload + footer, flags


def valid_resource_alias(alias: str) -> bool:
    if not alias.startswith(":/") or len(alias) <= 2:
        return False
    if "\\" in alias or "\x00" in alias or ":" in alias[2:]:
        return False
    return all(part not in ("", ".", "..") and len(part.encode("utf-8")) <= 63
               for part in alias[2:].split("/"))


def load_resources(manifest_path: pathlib.Path | None, image_end: int) -> list[tuple[bytes, pathlib.Path, int, int]]:
    """Resolve manifest sources relative to manifest.json, never to the working directory."""
    if manifest_path is None:
        return []
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise ValueError(f"cannot read manifest {manifest_path}: {exc}") from exc
    mapping = manifest.get("resources", {})
    if not isinstance(mapping, dict):
        raise ValueError("manifest.resources must be an object of alias-to-source mappings")
    if len(mapping) > MAX_RESOURCES:
        raise ValueError(f"manifest.resources exceeds {MAX_RESOURCES} entries")
    result = []
    offset = image_end
    table_bytes = 0
    for alias, source in sorted(mapping.items()):
        if not isinstance(alias, str) or not valid_resource_alias(alias):
            raise ValueError(f"invalid resource alias: {alias!r}; expected :/folder/file")
        name = alias.encode("utf-8")
        if len(name) > MAX_RESOURCE_ALIAS_BYTES:
            raise ValueError(f"resource alias is longer than {MAX_RESOURCE_ALIAS_BYTES} UTF-8 bytes: {alias!r}")
        if not isinstance(source, str) or not source.strip():
            raise ValueError(f"resource {alias!r} must specify a source file")
        path = pathlib.Path(source).expanduser()
        if not path.is_absolute():
            path = manifest_path.parent / path
        try:
            if not path.is_file():
                raise ValueError(f"resource {alias!r} is not a file: {path}")
            size = path.stat().st_size
        except OSError as exc:
            raise ValueError(f"resource {alias!r} cannot be accessed: {exc}") from exc
        if size < 0 or size > 0xFFFFFFFF or offset + size > 0xFFFFFFFF:
            raise ValueError(f"resource {alias!r} exceeds the 32-bit .lexx offset/size limit")
        if result and name.startswith(result[-1][0] + b"/"):
            raise ValueError(f"resource {alias!r} conflicts with a parent file alias")
        result.append((name, path, offset, size))
        offset += size
        table_bytes += RESOURCE_RECORD.size + len(name)
    if len(result) > LEGACY_RESOURCE_LIMIT:
        table_bytes += len(result) * 4  # u32 record offsets for LXR2
    if table_bytes > MAX_RESOURCE_TABLE_BYTES:
        raise ValueError("resource directory exceeds its size limit")
    return result


def append_resources(output, entries: list[tuple[bytes, pathlib.Path, int, int]]) -> int:
    """Stream raw files directly to output; only the small directory is held in RAM."""
    if not entries:
        return 0
    directory = bytearray()
    indexed = len(entries) > LEGACY_RESOURCE_LIMIT
    record_offsets = []
    written = 0
    for alias, path, offset, expected_size in entries:
        with path.open("rb") as source:
            remaining = expected_size
            while remaining:
                block = source.read(min(65536, remaining))
                if not block:
                    raise ValueError(f"resource changed during packing: {path}")
                output.write(block)
                remaining -= len(block)
        written += expected_size
        if indexed:
            record_offsets.append(len(directory))
        directory.extend(RESOURCE_RECORD.pack(offset, expected_size, len(alias)))
        directory.extend(alias)
    if indexed:
        for record_offset in record_offsets:
            directory.extend(struct.pack("<I", record_offset))
    if len(directory) > MAX_RESOURCE_TABLE_BYTES:
        raise ValueError("resource index exceeds its size limit")
    output.write(directory)
    output.write(RESOURCE_FOOTER.pack(b"LXR2" if indexed else b"LXRS",
                                      len(entries), len(directory), zlib.crc32(directory)))
    return written + len(directory) + RESOURCE_FOOTER.size


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Pack a flat RV32I binary into a LexxOS .lexx executable")
    parser.add_argument("input", type=pathlib.Path, help="input flat .bin produced by objcopy")
    parser.add_argument("output", type=pathlib.Path, help="output .lexx file")
    parser.add_argument("--manifest", type=pathlib.Path,
                        help="application manifest JSON to embed (build UTC timestamp is generated)")
    parser.add_argument("--entry", type=parse_integer, default=0, help="entry offset inside guest memory, default: 0")
    parser.add_argument("--memory", type=parse_integer, default=64 * 1024, help="guest memory size, default: 65536")
    parser.add_argument("--stack", type=parse_integer, default=4096,
                        help="guest stack reservation in bytes, default: 4096")
    parser.add_argument("--elf", type=pathlib.Path,
                        help="linked ELF for checking allocated .text/.data/.bss and stack against --memory")
    parser.add_argument(
        "--cap",
        action="append",
        default=None,
        help="required capability, comma-separated; default: console,keyboard",
    )
    args = parser.parse_args(argv)

    image = args.input.read_bytes()
    image_size = len(image)
    if args.memory <= 0 or args.memory > MAX_GUEST_BYTES:
        parser.error(f"--memory must be 1..{MAX_GUEST_BYTES} bytes")
    if args.stack <= 0:
        parser.error("--stack must be positive")
    memory_size = align_up(args.memory, 4)
    stack_size = align_up(args.stack, 4)
    capabilities = parse_capabilities(args.cap if args.cap is not None else DEFAULT_CAPABILITIES)

    if image_size == 0:
        parser.error("input binary is empty")
    if image_size > memory_size:
        parser.error(f"image size {image_size} exceeds guest memory {memory_size}")
    if args.entry < 0 or args.entry + 4 > memory_size or (args.entry & 0x03) != 0:
        parser.error("entry must be 4-byte aligned and point inside guest memory")
    if stack_size > memory_size - image_size:
        parser.error("image and declared stack overlap inside guest memory")
    if args.elf is not None:
        try:
            static_end = elf_allocated_end(args.elf)
        except (OSError, ValueError) as exc:
            parser.error(f"invalid --elf input: {exc}")
        if static_end > memory_size - stack_size:
            parser.error(
                f"ELF static sections ({static_end} bytes) and stack ({stack_size} bytes) exceed guest memory ({memory_size} bytes)")

    try:
        metadata, input_flags = pack_manifest(args.manifest, capabilities=capabilities,
                                              memory_size=memory_size, stack_size=stack_size) if args.manifest is not None else (b"", 0)
        resources = load_resources(args.manifest, HEADER_SIZE + image_size)
        if resources:
            data_end = resources[-1][2] + resources[-1][3]
            table_size = sum(RESOURCE_RECORD.size + len(name) for name, *_ in resources)
            if len(resources) > LEGACY_RESOURCE_LIMIT:
                table_size += len(resources) * 4
            if data_end + table_size + RESOURCE_FOOTER.size + len(metadata) > 0xFFFFFFFF:
                raise ValueError(".lexx exceeds 4 GiB; choose fewer assets")
    except ValueError as exc:
        parser.error(str(exc))
    header = build_header(
        capabilities=capabilities,
        entry=args.entry,
        image_size=image_size,
        memory_size=memory_size,
        stack_size=stack_size,
        bss_size=max(0, memory_size - image_size),
        flags=input_flags,
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)

    try:
        with args.output.open("wb") as output:
            output.write(header)
            output.write(image)
            resources_size = append_resources(output, resources)
            output.write(metadata)
    except (OSError, ValueError) as exc:
        args.output.unlink(missing_ok=True)
        parser.error(str(exc))
    total = HEADER_SIZE + image_size + resources_size + len(metadata)
    print(f"wrote {args.output} ({total} bytes, guest memory {memory_size} bytes, "
          f"manifest {'yes' if metadata else 'no'}, resources {len(resources)})")

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
