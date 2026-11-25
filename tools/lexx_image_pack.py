#!/usr/bin/env python3
"""Convert a PNG/JPEG image into the small LXIM RGB565 embedded-resource format.

Pillow is needed only when authoring assets on a PC, never for building the OS
or packaging the already generated .lxim files.
"""
import argparse
from pathlib import Path
import struct
import zlib


def encode_lxim(width: int, height: int, rgb: bytes, alpha: bytes | None = None) -> bytes:
    if width <= 0 or height <= 0 or width > 1024 or height > 1024 or width * height > 1048576:
        raise ValueError("image exceeds the LXIM authoring ceiling (1024x1024)")
    if len(rgb) != width * height * 3:
        raise ValueError("RGB888 input size does not match image dimensions")
    if alpha is not None and len(alpha) != width * height:
        raise ValueError("alpha8 input size does not match image dimensions")
    payload = bytearray()
    for i in range(0, len(rgb), 3):
        r, g, b = rgb[i:i + 3]
        pixel = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        payload.extend(struct.pack("<H", pixel))
    if alpha is not None:
        payload.extend(alpha)
    header = b"LXIM" + struct.pack("<HHHHI", width, height, 1, 1 if alpha is not None else 0, zlib.crc32(payload))
    return header + payload


def main() -> int:
    parser = argparse.ArgumentParser(description="Pack RGBA image as LXIM RGB565 resource")
    parser.add_argument("input", type=Path, help="PNG or another Pillow-supported image")
    parser.add_argument("output", type=Path, help=".lxim file included by app manifest")
    parser.add_argument("--width", type=int, required=True)
    parser.add_argument("--height", type=int, required=True)
    parser.add_argument("--alpha", action="store_true", help="preserve alpha8 instead of flattening against black")
    args = parser.parse_args()
    try:
        from PIL import Image, ImageOps
    except ImportError:
        parser.error("Pillow is required to convert images: python3 -m pip install Pillow")
    try:
        # Transparency stays planar alpha8 when explicitly requested.
        with Image.open(args.input) as source:
            image = ImageOps.contain(source.convert("RGBA"), (args.width, args.height),
                                     method=Image.Resampling.LANCZOS)
        canvas = Image.new("RGBA", (args.width, args.height), (0, 0, 0, 0 if args.alpha else 255))
        canvas.alpha_composite(image, ((args.width - image.width) // 2,
                                       (args.height - image.height) // 2))
        data = encode_lxim(args.width, args.height, canvas.convert("RGB").tobytes(),
                           canvas.getchannel("A").tobytes() if args.alpha else None)
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data)
    print(f"wrote {args.output}: {args.width}x{args.height} RGB565 ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
