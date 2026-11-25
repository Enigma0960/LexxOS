#!/usr/bin/env python3
"""Assemble a Lyra SD image from pinned vendor components and built LexxOS.

Only regular files are accepted as outputs. No device access or flashing.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import uuid
import zlib

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import rk3506_pack_fit as fit

SECTOR = 512
MIB = 1024 * 1024
TYPES = ["C235E4D3-E8E7-4517-8256-3122254620A2",
         "B6E57622-0B69-46AF-AD2C-DADAF4E2E8CA",
         "5ED76E24-AE0D-4330-A411-71D190D57FA9"]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def validate_loader(data):
    """RKNS v2 layout from pinned vendor U-Boot tools/rkcommon.c."""
    if len(data) < 2048 or data[:4] != b"RKNS":
        raise ValueError("missing RK3506 SD idblock header")
    descriptor, flags = struct.unpack_from("<II", data, 8)
    count, hash_offset = descriptor >> 16, (descriptor & 0xffff) * 4
    if count != 2 or hash_offset != 1536 or flags != 1:
        raise ValueError("unsupported RK3506 idblock header")
    if hashlib.sha256(data[:hash_offset]).digest() != data[hash_offset:hash_offset + 32]:
        raise ValueError("idblock header checksum mismatch")
    end = 2048
    for index in range(count):
        entry = 120 + index * 88
        extent = struct.unpack_from("<I", data, entry)[0]
        start, size = (extent & 0xffff) * 512, (extent >> 16) * 512
        if start != end or size == 0 or start + size > len(data):
            raise ValueError("invalid DDR/SPL extent")
        if hashlib.sha256(data[start:start + size]).digest() != data[entry + 24:entry + 56]:
            raise ValueError("DDR/SPL checksum mismatch")
        end = start + size
    if end != len(data):
        raise ValueError("unexpected trailing data in SD loader")


def components():
    lock = json.loads((Path(__file__).with_name("components.json")).read_text())
    directory = ROOT / "third_party/rk3506-luckfox-lurra"
    blobs = {}
    for name, expected in lock["files"].items():
        data = (directory / name).read_bytes()
        if len(data) != expected["size"] or sha(data) != expected["sha256"]:
            raise ValueError(f"vendor component checksum mismatch: {name}")
        blobs[name] = data
    validate_loader(blobs["sd-loader.bin"])
    return lock, blobs


def boot_image(payload, blobs):
    """Retain vendor FIT ABI, DTB and resource without retaining a Linux payload."""
    header = bytearray(blobs["fit-header.bin"])
    props = fit.parse_properties(header)
    total = fit.get_int(header, props, ("",), "totalsize")
    if total > fit.BOOT_PARTITION_SIZE:
        raise ValueError("boot FIT exceeds partition")
    image = header + bytearray(total - len(header))
    for name, filename in [("fdt", "board.dtb"), ("resource", "resource.img")]:
        path = ("", "images", name)
        start = fit.get_int(image, props, path, "data-position")
        size = fit.get_int(image, props, path, "data-size")
        if size != len(blobs[filename]) or start + size > total:
            raise ValueError("invalid retained FIT component extent")
        image[start:start + size] = blobs[filename]
    # Make the empty kernel slot a valid input to the existing checked packer.
    path = ("", "images", "kernel")
    size = fit.get_int(image, props, path, "data-size")
    fit.set_prop(image, props, path + ("hash",), "value", hashlib.sha256(bytes(size)).digest())
    return fit.pack(bytes(image), payload, 0x02000000)


def validate_uboot(image):
    if len(image) != 4 * MIB or image[:2 * MIB] != image[2 * MIB:]:
        raise ValueError("U-Boot must contain two identical 2 MiB slots")
    blob = bytearray(image[:2 * MIB])
    props = fit.parse_properties(blob)
    names = [path[-1] for path in props if len(path) == 3 and path[1] == "images"]
    if not names:
        raise ValueError("U-Boot FIT contains no images")
    if not any(fit.get_prop(blob, props, ("", "images", name), "os").lower() == b"u-boot\0"
               for name in names if "os" in props[("", "images", name)]):
        raise ValueError("FIT has no U-Boot payload")
    for name in names:
        fit.read_image(blob, props, name)


def gpt(sectors):
    if sectors < 65 * MIB // SECTOR or sectors > 0xffffffff:
        raise ValueError("image size must be between 65 MiB and 2 TiB")
    entries = bytearray(128 * 128)
    for i, (name, start, end) in enumerate([
        ("uboot", 8192, 16383), ("boot", 16384, 40959),
        ("rootfs", 65536, sectors - 34),
    ]):
        identity = uuid.uuid5(uuid.NAMESPACE_URL, "lexxos/lyra/" + name).bytes_le
        entry = struct.pack("<16s16sQQQ72s", uuid.UUID(TYPES[i]).bytes_le,
                            identity, start, end, 0, name.encode("utf-16le"))
        entries[i * 128:(i + 1) * 128] = entry
    disk_id = uuid.uuid5(uuid.NAMESPACE_URL, "lexxos/lyra/sd-v1").bytes_le

    def header(current, backup, table):
        data = bytearray(struct.pack("<8sIIIIQQQQ16sQIII", b"EFI PART", 0x10000,
                                    92, 0, 0, current, backup, 34, sectors - 34,
                                    disk_id, table, 128, 128, zlib.crc32(entries)))
        struct.pack_into("<I", data, 16, zlib.crc32(data))
        return bytes(data).ljust(SECTOR, b"\0")

    mbr = bytearray(SECTOR)
    mbr[446:462] = struct.pack("<B3sB3sII", 0, b"\0\2\0", 0xee,
                               b"\xff\xff\xff", 1, sectors - 1)
    mbr[510:] = b"\x55\xaa"
    return [(0, bytes(mbr)), (512, header(1, sectors - 1, 2)), (1024, bytes(entries)),
            ((sectors - 33) * 512, bytes(entries)),
            ((sectors - 1) * 512, header(sectors - 1, 1, sectors - 33))]


def verify(path):
    size = path.stat().st_size
    if size % SECTOR:
        raise ValueError("unaligned SD image")
    sectors = size // SECTOR
    with path.open("rb") as stream:
        for offset, expected in gpt(sectors):
            stream.seek(offset)
            if stream.read(len(expected)) != expected:
                raise ValueError(f"GPT/MBR mismatch at {offset:#x}")
        lock, blobs = components()
        stream.seek(64 * SECTOR)
        if stream.read(len(blobs["sd-loader.bin"])) != blobs["sd-loader.bin"]:
            raise ValueError("SD loader mismatch")
        stream.seek(4 * MIB)
        validate_uboot(stream.read(4 * MIB))
        stream.seek(8 * MIB)
        boot = bytearray(stream.read(12 * MIB))
        props = fit.parse_properties(boot)
        for name, filename in [("kernel", None), ("fdt", "board.dtb"), ("resource", "resource.img")]:
            start, length = fit.read_image(boot, props, name)
            if filename and boot[start:start + length] != blobs[filename]:
                raise ValueError(f"vendor {name} changed")
        kernel = ("", "images", "kernel")
        for key in ["load", "entry"]:
            if fit.get_int(boot, props, kernel, key) != 0x02000000:
                raise ValueError("wrong LexxOS handoff address")
    manifest_path = path.with_suffix(".json")
    if manifest_path.is_file():
        manifest = json.loads(manifest_path.read_text())
        with path.open("rb") as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
        if manifest["size"] != size or manifest["sha256"] != digest:
            raise ValueError("complete SD image checksum mismatch")


def atomic_write(path, data):
    path = path.absolute()
    if path.is_symlink() or (path.exists() and not path.is_file()):
        raise ValueError(f"output must be a regular file: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as stream:
        temporary = Path(stream.name)
        try:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
            os.replace(temporary, path)
        finally:
            temporary.unlink(missing_ok=True)


def build(payload_path, uboot_path, output, size_mib=128, rootfs=None, size_bytes=None):
    lock, blobs = components()
    payload = payload_path.read_bytes()
    boot = boot_image(payload, blobs)
    uboot = uboot_path.read_bytes()
    validate_uboot(uboot)
    image_bytes = size_bytes if size_bytes is not None else size_mib * MIB
    if image_bytes % SECTOR:
        raise ValueError("image size must be a multiple of 512 bytes")
    sectors = image_bytes // SECTOR
    regions = gpt(sectors) + [(64 * SECTOR, blobs["sd-loader.bin"]),
                              (4 * MIB, uboot), (8 * MIB, boot)]
    root = rootfs.read_bytes() if rootfs else b""
    if len(root) > (sectors - 33 - 65536) * SECTOR:
        raise ValueError("rootfs does not fit; increase --size-mib")
    if root:
        regions.append((32 * MIB, root))
    output = output.absolute()
    if output.suffix != ".img":
        raise ValueError("SD output filename must end in .img")
    if output.is_symlink() or (output.exists() and not output.is_file()):
        raise ValueError("output must be a regular file")
    if output.resolve() in {payload_path.resolve(), uboot_path.resolve(), *( [rootfs.resolve()] if rootfs else [])}:
        raise ValueError("output would overwrite an input")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=output.parent, delete=False) as stream:
        temporary = Path(stream.name)
        try:
            stream.truncate(sectors * SECTOR)
            for offset, data in regions:
                stream.seek(offset)
                stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
            verify(temporary)
            with temporary.open("rb") as reader:
                digest = hashlib.file_digest(reader, "sha256").hexdigest()
            os.replace(temporary, output)
        finally:
            temporary.unlink(missing_ok=True)
    manifest = {"schema": 1, "board": lock["board"], "size": sectors * SECTOR,
                "sha256": digest, "kernel_sha256": sha(payload), "uboot_sha256": sha(uboot),
                "boot_sha256": sha(boot), "components": lock,
                "rootfs_sha256": sha(root) if root else None,
                "rootfs_policy": "preserved vendor image" if root else "empty reserved partition",
                "hardware_verified": False}
    atomic_write(output.with_suffix(".json"), (json.dumps(manifest, indent=2) + "\n").encode())
    atomic_write(output.with_suffix(".img.sha256"), f"{digest}  {output.name}\n".encode())
    print(f"Created and verified {output} ({image_bytes} bytes), SHA-256 {digest}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kernel", type=Path)
    parser.add_argument("--uboot", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--size-mib", type=int, default=128)
    parser.add_argument("--size-bytes", type=int, help="exact card size; overrides --size-mib, places backup GPT at card end")
    parser.add_argument("--rootfs", type=Path)
    parser.add_argument("--verify", type=Path)
    parser.add_argument("--boot-only", action="store_true")
    args = parser.parse_args()
    try:
        if args.verify:
            verify(args.verify)
            print("GPT, SD loader, U-Boot copies, FIT hashes and board resources: verified")
        elif args.boot_only and args.kernel and args.output:
            _, blobs = components()
            atomic_write(args.output, boot_image(args.kernel.read_bytes(), blobs))
        elif args.kernel and args.uboot and args.output:
            build(args.kernel, args.uboot, args.output, args.size_mib, args.rootfs, args.size_bytes)
        else:
            parser.error("use --verify IMAGE or --kernel BIN --uboot IMG --output IMG")
    except (OSError, ValueError, struct.error, KeyError) as exc:
        parser.exit(1, f"lyra: {exc}\n")


if __name__ == "__main__":
    main()
