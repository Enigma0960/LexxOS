#!/usr/bin/env python3
"""Create lexxos.img for the stock Lyra RK3506 SDMMC FIT boot partition.

A vendor boot.img is supplied by the user at build time, not stored in LexxOS.
We preserve the vendor DTB, resource image and fixed external-data positions;
only the kernel bytes and its FIT properties are changed. No Linux kernel runs.

This intentionally targets the checked Luckfox_Lyra_MircoSD_250717 boot.img
layout. Unknown templates or signed configurations are refused.
"""

import argparse
import hashlib
import struct
from pathlib import Path

FDT_MAGIC = 0xD00DFEED
BEGIN_NODE = 1
END_NODE = 2
PROP = 3
NOP = 4
END = 9
BOOT_PARTITION_SIZE = 0x6000 * 512  # parameter.txt: 0x6000 sectors (12 MiB)


class FitError(ValueError):
    """Invalid FIT image or a template this packer does not understand."""


def u32(blob: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from(">I", blob, offset)[0]


def parse_properties(blob: bytearray) -> dict[tuple[str, ...], dict[str, tuple[int, int]]]:
    if len(blob) < 40 or u32(blob, 0) != FDT_MAGIC:
        raise FitError("template is not a FIT/FDT image")
    total, struct_off, strings_off, _, version, _, _, strings_size, struct_size = struct.unpack_from(
        ">9I", blob, 4
    )
    if version != 17 or total > len(blob) or total < 40:
        raise FitError("unsupported FIT metadata header")
    if struct_off + struct_size > total or strings_off + strings_size > total:
        raise FitError("FIT metadata extends beyond its header")
    strings = blob[strings_off: strings_off + strings_size]
    stack: list[str] = []
    props: dict[tuple[str, ...], dict[str, tuple[int, int]]] = {}
    cursor = struct_off
    terminated = False
    while cursor < struct_off + struct_size:
        tag = u32(blob, cursor)
        cursor += 4
        if tag == BEGIN_NODE:
            try:
                end = blob.index(0, cursor, struct_off + struct_size)
            except ValueError as exc:
                raise FitError("unterminated FIT node name") from exc
            stack.append(blob[cursor:end].decode("ascii"))
            cursor = (end + 4) & ~3
            props[tuple(stack)] = {}
        elif tag == END_NODE:
            if not stack:
                raise FitError("unexpected FIT end-node")
            stack.pop()
        elif tag == PROP:
            size, name_offset = struct.unpack_from(">II", blob, cursor)
            cursor += 8
            if cursor + size > struct_off + struct_size or name_offset >= len(strings):
                raise FitError("malformed FIT property")
            try:
                name_end = strings.index(0, name_offset)
            except ValueError as exc:
                raise FitError("unterminated FIT property name") from exc
            name = strings[name_offset:name_end].decode("ascii")
            current = props[tuple(stack)]
            if name in current:
                raise FitError(f"duplicate FIT property {name}")
            current[name] = (cursor, size)
            cursor = (cursor + size + 3) & ~3
        elif tag == NOP:
            continue
        elif tag == END:
            if stack:
                raise FitError("unterminated FIT node")
            terminated = True
            break
        else:
            raise FitError(f"unknown FIT tag {tag}")
    if not terminated:
        raise FitError("missing FIT end marker")
    return props


def get_prop(blob: bytearray, props: dict, path: tuple[str, ...], name: str) -> bytes:
    try:
        start, size = props[path][name]
    except KeyError as exc:
        raise FitError(f"missing property {'/'.join(path)}/{name}") from exc
    return bytes(blob[start: start + size])


def get_int(blob: bytearray, props: dict, path: tuple[str, ...], name: str) -> int:
    raw = get_prop(blob, props, path, name)
    if len(raw) != 4:
        raise FitError(f"expected uint32 for {'/'.join(path)}/{name}")
    return struct.unpack(">I", raw)[0]


def set_prop(blob: bytearray, props: dict, path: tuple[str, ...], name: str, value: bytes) -> None:
    start, size = props[path][name]
    if len(value) != size:
        raise FitError(f"refusing to resize FIT property {'/'.join(path)}/{name}")
    blob[start: start + size] = value


def set_int(blob: bytearray, props: dict, path: tuple[str, ...], name: str, value: int) -> None:
    set_prop(blob, props, path, name, struct.pack(">I", value))


def read_image(blob: bytearray, props: dict, image: str) -> tuple[int, int]:
    path = ("", "images", image)
    start = get_int(blob, props, path, "data-position")
    size = get_int(blob, props, path, "data-size")
    if size == 0 or start < u32(blob, 4) or start + size > len(blob):
        raise FitError(f"invalid external data for {image}")
    algo = get_prop(blob, props, path + ("hash",), "algo")
    if algo != b"sha256\0":
        raise FitError(f"unsupported hash for {image}")
    expected = get_prop(blob, props, path + ("hash",), "value")
    if expected != hashlib.sha256(blob[start: start + size]).digest():
        raise FitError(f"SHA-256 mismatch in template: {image}")
    return start, size


def pack(template: bytes, payload: bytes, address: int) -> bytes:
    if not (0 <= address <= 0xFFFFFFFF) or address & 3:
        raise FitError("load/entry address must be a 4-byte-aligned ARM32 address")
    if not payload:
        raise FitError("ARM payload is empty")
    if len(payload) > 0xFFFFFFFF:
        raise FitError("ARM payload is too large")
    if len(template) > BOOT_PARTITION_SIZE:
        raise FitError("template does not fit in the 12 MiB boot partition")
    blob = bytearray(template)
    props = parse_properties(blob)
    if get_int(blob, props, ("",), "totalsize") != len(blob):
        raise FitError("vendor root totalsize does not match boot.img length")
    if get_prop(blob, props, ("", "configurations"), "default") != b"conf\0":
        raise FitError("unexpected FIT default configuration")
    config = ("", "configurations", "conf")
    if get_prop(blob, props, config, "kernel") != b"kernel\0":
        raise FitError("FIT conf does not reference expected kernel")
    if get_prop(blob, props, config, "fdt") != b"fdt\0":
        raise FitError("FIT conf does not reference expected fdt")
    if get_prop(blob, props, config, "multi") != b"resource\0":
        raise FitError("FIT conf does not reference expected resource")
    if any("signature" in path and "value" in p for path, p in props.items()):
        raise FitError("signed FIT: modifying it would invalidate its signature")
    kernel = ("", "images", "kernel")
    for name, expected in (("type", b"kernel\0"), ("arch", b"arm\0"),
                           ("os", b"linux\0"), ("compression", b"none\0")):
        if get_prop(blob, props, kernel, name) != expected:
            raise FitError(f"unexpected kernel {name} in template")
    fdt_position, fdt_size = read_image(blob, props, "fdt")
    kernel_position, old_kernel_size = read_image(blob, props, "kernel")
    resource_position, resource_size = read_image(blob, props, "resource")
    if not (fdt_position + fdt_size <= kernel_position < resource_position):
        raise FitError("unexpected FIT image ordering")
    capacity = resource_position - kernel_position
    if old_kernel_size > capacity or len(payload) > capacity:
        raise FitError(f"payload exceeds reserved kernel slot ({capacity} bytes)")
    # Discard the original Linux payload, including padding; preserve DTB and
    # resource block byte-for-byte, using the original external data positions.
    blob[kernel_position:resource_position] = b"\0" * capacity
    blob[kernel_position: kernel_position + len(payload)] = payload
    set_int(blob, props, kernel, "data-size", len(payload))
    set_int(blob, props, kernel, "load", address)
    set_int(blob, props, kernel, "entry", address)
    set_prop(blob, props, kernel + ("hash",), "value", hashlib.sha256(payload).digest())
    # Verify final FIT and ensure the two vendor subimages are not changed.
    updated = parse_properties(blob)
    if read_image(blob, updated, "kernel") != (kernel_position, len(payload)):
        raise FitError("generated kernel verification failed")
    if read_image(blob, updated, "fdt") != (fdt_position, fdt_size):
        raise FitError("generated fdt verification failed")
    if read_image(blob, updated, "resource") != (resource_position, resource_size):
        raise FitError("generated resource verification failed")
    return bytes(blob)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--template", type=Path, required=True,
                        help="stock Luckfox_Lyra_MircoSD_250717/boot.img")
    parser.add_argument("--input", type=Path, required=True, help="lexxos.bin")
    parser.add_argument("--output", type=Path, required=True, help="lexxos.img")
    parser.add_argument("--address", type=lambda s: int(s, 0), default=0x02000000)
    args = parser.parse_args()
    try:
        image = pack(args.template.read_bytes(), args.input.read_bytes(), args.address)
    except (FitError, OSError) as exc:
        parser.error(str(exc))
    args.output.write_bytes(image)
    print(f"Created {args.output}: FIT {len(image)} bytes, ARM load/entry {args.address:#010x}")
    print("SHA-256 kernel, vendor DTB and resource: verified")


if __name__ == "__main__":
    main()
