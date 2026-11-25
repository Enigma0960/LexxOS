"""Image packer contract independent of Pillow."""
import importlib.util
from pathlib import Path
import struct
import unittest
import zlib

SCRIPT = Path(__file__).resolve().parents[2] / "tools" / "lexx_image_pack.py"
SPEC = importlib.util.spec_from_file_location("lexx_image_pack", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
module = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(module)


class ImagePackerTests(unittest.TestCase):
    def test_rgb565_and_crc(self):
        data = module.encode_lxim(2, 1, bytes([255, 0, 0, 0, 255, 0]))
        self.assertEqual(data[:4], b"LXIM")
        self.assertEqual(struct.unpack_from("<HHHH", data, 4), (2, 1, 1, 0))
        self.assertEqual(data[16:], b"\x00\xf8\xe0\x07")
        self.assertEqual(struct.unpack_from("<I", data, 12)[0], zlib.crc32(data[16:]))

    def test_alpha8_payload_and_crc(self):
        data = module.encode_lxim(2, 1, bytes([255, 0, 0, 0, 0, 255]), bytes([0, 128]))
        self.assertEqual(struct.unpack_from("<HHHH", data, 4), (2, 1, 1, 1))
        self.assertEqual(data[16:], b"\x00\xf8\x1f\x00\x00\x80")
        self.assertEqual(struct.unpack_from("<I", data, 12)[0], zlib.crc32(data[16:]))

    def test_alpha8_size_validation(self):
        with self.assertRaises(ValueError):
            module.encode_lxim(2, 1, bytes(6), bytes(1))

    def test_reject_invalid_size(self):
        with self.assertRaises(ValueError):
            module.encode_lxim(321, 1, bytes(321 * 3))


if __name__ == "__main__":
    unittest.main()
