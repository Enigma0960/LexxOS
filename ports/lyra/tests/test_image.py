"""Host checks; never open a block device."""
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

spec = importlib.util.spec_from_file_location("lyra_image", Path(__file__).parents[1] / "build.py")
lyra = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lyra)


class ImageTests(unittest.TestCase):
    def vendor_blobs(self):
        try:
            _, blobs = lyra.components()
        except FileNotFoundError:
            self.skipTest("optional Lyra vendor components absent")
        return blobs

    def test_gpt_crc_and_partition_bounds(self):
        sectors = 7866368  # Actual reference card geometry, not rounded MiB.
        regions = dict(lyra.gpt(sectors))
        for lba, other in [(1, sectors - 1), (sectors - 1, 1)]:
            header = bytearray(regions[lba * 512][:92])
            crc = struct.unpack_from("<I", header, 16)[0]
            struct.pack_into("<I", header, 16, 0)
            self.assertEqual(zlib.crc32(header), crc)
            self.assertEqual(struct.unpack_from("<QQ", header, 24), (lba, other))
            table_lba = struct.unpack_from("<Q", header, 72)[0]
            self.assertEqual(zlib.crc32(regions[table_lba * 512]), struct.unpack_from("<I", header, 88)[0])
        entries = regions[1024]
        self.assertEqual(entries, regions[(sectors - 33) * 512])
        self.assertEqual(struct.unpack_from("<QQ", entries, 2 * 128 + 32), (65536, sectors - 34))
        self.assertEqual(regions[0][510:], b"\x55\xaa")

    def test_small_image_rejected(self):
        with self.assertRaises(ValueError):
            lyra.gpt(64 * lyra.MIB // 512)

    def test_boot_matches_existing_packer_byte_for_byte(self):
        blobs = self.vendor_blobs()
        payload = b"ARM test payload" * 100
        template_path = lyra.ROOT / "third_party/rk3506-luckfox-lurra/boot.img"
        if not template_path.is_file():
            self.skipTest("optional Lyra vendor FIT template absent")
        template = template_path.read_bytes()
        self.assertEqual(lyra.boot_image(payload, blobs), lyra.fit.pack(template, payload, 0x02000000))

    def test_bad_board_resource_rejected(self):
        blobs = self.vendor_blobs()
        blobs["board.dtb"] = bytes(len(blobs["board.dtb"]))
        with self.assertRaises(ValueError):
            lyra.boot_image(b"ARM", blobs)

    def test_loader_hashes_and_trailing_data(self):
        blobs = self.vendor_blobs()
        loader = blobs["sd-loader.bin"]
        for offset in [200, 2048, 22528]:
            corrupt = bytearray(loader)
            corrupt[offset] ^= 1
            with self.assertRaises(ValueError):
                lyra.validate_loader(corrupt)
        with self.assertRaises(ValueError):
            lyra.validate_loader(loader + bytes(512))

    def test_empty_and_oversized_kernel_rejected(self):
        blobs = self.vendor_blobs()
        for payload in [b"", bytes(12 * lyra.MIB)]:
            with self.assertRaises(ValueError):
                lyra.boot_image(payload, blobs)

    def test_boot_fit_is_not_uboot(self):
        blobs = self.vendor_blobs()
        boot = lyra.boot_image(b"ARM", blobs)[:2 * lyra.MIB]
        with self.assertRaises(ValueError):
            lyra.validate_uboot(boot * 2)

    def test_gpt_corruption_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "sd.img"
            with path.open("wb") as out:
                out.truncate(128 * lyra.MIB)
                for offset, data in lyra.gpt(128 * lyra.MIB // 512):
                    out.seek(offset)
                    out.write(data)
                out.seek(512 + 16)
                out.write(b"BAD!")
            with self.assertRaisesRegex(ValueError, "GPT/MBR"):
                lyra.verify(path)

    def test_output_symlink_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            original = Path(tmp) / "original"
            original.write_bytes(b"keep")
            link = Path(tmp) / "link"
            link.symlink_to(original)
            with self.assertRaises(ValueError):
                lyra.atomic_write(link, b"replace")
            self.assertEqual(original.read_bytes(), b"keep")


if __name__ == "__main__":
    unittest.main()
