#!/usr/bin/env python3
"""Wrap a bare-metal ARM payload in the legacy U-Boot image format.

The Linux OS field is intentional: 'bootm' uses the ARM Linux handoff path to
clean up CPU state before jumping to our freestanding payload. No Linux runs.
"""
import argparse
import struct
import time
import zlib
from pathlib import Path

HEADER = struct.Struct(">7I4B32s")  # 64 bytes, U-Boot legacy image_header_t
MAGIC = 0x27051956
OS_LINUX = 5
ARCH_ARM = 2
TYPE_KERNEL = 2
COMP_NONE = 0


def number(value: str) -> int:
    return int(value, 0)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--load", type=number, required=True)
    parser.add_argument("--entry", type=number, required=True)
    args = parser.parse_args()

    payload = args.input.read_bytes()
    for value in (args.load, args.entry, len(payload)):
        if not 0 <= value <= 0xffffffff:
            parser.error("uImage fields must fit in uint32")
    name = b"LexxOS RK3506 bare-metal bootstrap"[:32].ljust(32, b"\0")
    fields = [MAGIC, 0, int(time.time()), len(payload), args.load, args.entry,
              zlib.crc32(payload) & 0xffffffff, OS_LINUX, ARCH_ARM,
              TYPE_KERNEL, COMP_NONE, name]
    zero_crc = HEADER.pack(*fields)
    fields[1] = zlib.crc32(zero_crc) & 0xffffffff
    args.output.write_bytes(HEADER.pack(*fields) + payload)
    print(f"Created {args.output}: {len(payload)} payload bytes at {args.load:#010x}")


if __name__ == "__main__":
    main()
