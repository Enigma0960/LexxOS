"""Check built-in screensaver resources without hardware or a guest compiler."""
from pathlib import Path
import struct
import unittest
import zlib

ASSETS = Path(__file__).resolve().parents[2] / "apps" / "sdk-demo" / "assets"

class GraphicsAssetTests(unittest.TestCase):
    def test_packaged_image_headers_and_pixels(self):
        for name, dimensions in (("lexxos.lxim", (124, 34)), ("banner.lxim", (64, 20))):
            with self.subTest(name=name):
                contents = (ASSETS / name).read_bytes()
                self.assertEqual(contents[:4], b"LXIM")
                width, height, fmt, flags, crc = struct.unpack_from("<HHHHI", contents, 4)
                self.assertEqual((width, height), dimensions)
                self.assertEqual((fmt, flags), (1, 0))
                self.assertEqual(len(contents), 16 + width * height * 2)
                self.assertEqual(zlib.crc32(contents[16:]), crc)
                self.assertGreater(len(set(contents[16:])), 2)

if __name__ == "__main__":
    unittest.main()
