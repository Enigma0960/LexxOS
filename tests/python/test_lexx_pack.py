"""Regression checks for the portable .lexx manifest trailer and legacy compatibility."""

from __future__ import annotations

import importlib.util
import json
import os
import pathlib
import struct
import tempfile
import unittest
from unittest import mock
import zlib

PACKER_PATH = pathlib.Path(__file__).resolve().parents[2] / "tools" / "lexx_pack.py"
SPEC = importlib.util.spec_from_file_location("lexx_pack", PACKER_PATH)
assert SPEC is not None and SPEC.loader is not None
packer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(packer)


class LexxPackManifestTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.folder = pathlib.Path(self.temp.name)
        self.binary = self.folder / "hello.bin"
        self.executable = self.folder / "hello.lexx"
        self.manifest = self.folder / "manifest.json"
        self.binary.write_bytes(b"\x13\x00\x00\x00")  # RV32I nop
        self.metadata = {
            "schema": 1,
            "id": "org.example.hello",
            "name": "Hello",
            "version": "1.2.3",
            "author": "Пример автора",
            "license": "MIT",
            "contacts": {"email": "dev@example.org"},
        }
        self.save_manifest()

    def save_manifest(self) -> None:
        self.manifest.write_text(json.dumps(self.metadata, ensure_ascii=False), encoding="utf-8")

    def test_guest_memory_limits_reject_oversubscription(self) -> None:
        for args in (["--memory", "65540"], ["--memory", "4096", "--stack", "4096"],
                     ["--memory", "-1"], ["--stack", "0"]):
            with self.subTest(args=args), self.assertRaises(SystemExit) as failure:
                packer.main([str(self.binary), str(self.executable), *args])
            self.assertEqual(failure.exception.code, 2)

    def test_elf_bss_is_included_in_static_memory_requirement(self) -> None:
        # Minimal little-endian RV32 ELF with one SHF_ALLOC NOBITS section.
        elf = self.folder / "test.elf"
        header = bytearray(52)
        header[:6] = b"\x7fELF\x01\x01"
        struct.pack_into("<H", header, 18, 243)  # EM_RISCV
        struct.pack_into("<I", header, 32, 52)  # e_shoff
        struct.pack_into("<HH", header, 46, 40, 1)
        section = struct.pack("<IIIIIIIIII", 0, 8, 2, 0x1800, 0, 0x1000, 0, 0, 4, 0)
        elf.write_bytes(header + section)
        self.assertEqual(packer.elf_allocated_end(elf), 0x2800)
        with self.assertRaises(SystemExit) as failure:
            packer.main([str(self.binary), str(self.executable), "--elf", str(elf),
                         "--memory", "12288", "--stack", "4096"])
        self.assertEqual(failure.exception.code, 2)
        packer.main([str(self.binary), str(self.executable), "--elf", str(elf),
                     "--memory", "16384", "--stack", "4096"])

    def test_stack_reservation_is_encoded_in_header(self) -> None:
        packer.main([str(self.binary), str(self.executable)])
        self.assertEqual(struct.unpack_from("<I", self.executable.read_bytes(), 36)[0], 4096)

        packer.main([str(self.binary), str(self.executable), "--stack", "8192"])
        self.assertEqual(struct.unpack_from("<I", self.executable.read_bytes(), 36)[0], 8192)

    def test_legacy_executable_keeps_original_header_and_size(self) -> None:
        packer.main([str(self.binary), str(self.executable)])
        data = self.executable.read_bytes()
        self.assertEqual(data[:4], b"LEXX")
        self.assertEqual(len(data), packer.HEADER_SIZE + 4)
        self.assertEqual(struct.unpack_from("<H", data, 4)[0], 1)
        self.assertEqual(data[64:], self.binary.read_bytes())

    def test_manifest_trailer_has_build_time_and_crc32(self) -> None:
        with mock.patch.dict(os.environ, {"SOURCE_DATE_EPOCH": "1700000000"}):
            packer.main([str(self.binary), str(self.executable), "--manifest", str(self.manifest)])
        data = self.executable.read_bytes()
        magic, length, crc = struct.unpack("<4sII", data[-12:])
        payload = data[-12 - length:-12]
        metadata = json.loads(payload.decode("utf-8"))
        self.assertEqual(magic, b"LXMF")
        self.assertEqual(crc, zlib.crc32(payload))
        self.assertEqual(metadata["build"]["timestamp"], "2023-11-14T22:13:20Z")
        self.assertEqual(metadata["author"], "Пример автора")
        self.assertEqual(data[64:68], self.binary.read_bytes())
        self.assertEqual(data[44:64], bytes(20))

    def test_manifest_input_events_sets_header_flag(self) -> None:
        self.metadata["input"] = {"mode": "events"}
        self.save_manifest()
        packer.main([str(self.binary), str(self.executable), "--manifest", str(self.manifest)])
        content = self.executable.read_bytes()
        self.assertEqual(struct.unpack_from("<I", content, 12)[0], packer.INPUT_EVENTS_FLAG)
        magic, size, _ = struct.unpack_from("<4sII", content, len(content) - 12)
        self.assertEqual(magic, b"LXMF")
        self.assertEqual(json.loads(content[-12 - size:-12])["input"]["mode"], "events")

    def test_manifest_input_mode_validation(self) -> None:
        for value in ({"mode": "other"}, {"mode": True}, {"mode": "events", "extra": 1}, []):
            with self.subTest(value=value):
                self.metadata["input"] = value
                self.save_manifest()
                with self.assertRaises(SystemExit):
                    packer.main([str(self.binary), str(self.executable), "--manifest", str(self.manifest)])

    def test_reproducible_timestamp(self) -> None:
        with mock.patch.dict(os.environ, {"SOURCE_DATE_EPOCH": "1700000000"}):
            packer.main([str(self.binary), str(self.executable), "--manifest", str(self.manifest)])
            first = self.executable.read_bytes()
            packer.main([str(self.binary), str(self.executable), "--manifest", str(self.manifest)])
            self.assertEqual(first, self.executable.read_bytes())

    def test_manifest_validation_rejects_wrong_schema(self) -> None:
        self.metadata["schema"] = "1"
        self.save_manifest()
        with self.assertRaises(SystemExit) as failure:
            packer.main([str(self.binary), str(self.executable), "--manifest", str(self.manifest)])
        self.assertEqual(failure.exception.code, 2)

    def test_build_field_must_be_generated(self) -> None:
        self.metadata["build"] = {"timestamp": "fake"}
        self.save_manifest()
        with self.assertRaises(SystemExit) as failure:
            packer.main([str(self.binary), str(self.executable), "--manifest", str(self.manifest)])
        self.assertEqual(failure.exception.code, 2)

    def test_oversized_metadata_rejected(self) -> None:
        self.metadata["description"] = "large" * 1200
        self.save_manifest()
        with self.assertRaises(SystemExit) as failure:
            packer.main([str(self.binary), str(self.executable), "--manifest", str(self.manifest)])
        self.assertEqual(failure.exception.code, 2)

    def test_embedded_resources_keep_binary_data_and_absolute_offsets(self) -> None:
        assets = self.folder / "assets"
        assets.mkdir()
        (assets / "image.png").write_bytes(b"\x89PNG\x00\xff\x00")
        (assets / "empty.txt").write_bytes(b"")
        self.metadata["resources"] = {
            ":/img/image.png": "assets/image.png",
            ":/empty.txt": "assets/empty.txt",
        }
        self.save_manifest()
        packer.main([str(self.binary), str(self.executable), "--manifest", str(self.manifest)])
        packed = self.executable.read_bytes()
        magic, length, checksum = struct.unpack_from("<4sII", packed, len(packed) - 12)
        self.assertEqual(magic, b"LXMF")
        manifest_start = len(packed) - 12 - length
        embedded = json.loads(packed[manifest_start:-12].decode())
        self.assertNotIn("resources", embedded)  # build paths must not be published
        self.assertEqual(checksum, zlib.crc32(packed[manifest_start:-12]))
        magic, count, table_bytes, crc = struct.unpack_from("<4sIII", packed, manifest_start - 16)
        self.assertEqual((magic, count), (b"LXRS", 2))
        table = packed[manifest_start - 16 - table_bytes:manifest_start - 16]
        self.assertEqual(zlib.crc32(table), crc)
        cursor = 0
        for expected_alias, expected_content in [
            (":/empty.txt", b""), (":/img/image.png", b"\x89PNG\x00\xff\x00")
        ]:
            offset, length, name_length = struct.unpack_from("<IIH", table, cursor)
            cursor += 10
            alias = table[cursor:cursor + name_length].decode()
            cursor += name_length
            self.assertEqual(alias, expected_alias)
            self.assertEqual(packed[offset:offset + length], expected_content)
        self.assertEqual(cursor, table_bytes)
        self.assertEqual(packed[64:68], self.binary.read_bytes())

    def test_large_catalog_uses_binary_search_index(self) -> None:
        asset = self.folder / "pixel.bin"
        asset.write_bytes(b"\x12\x34")
        self.metadata["resources"] = {
            f":/sprites/p{i:03d}/idle.lxim": "pixel.bin" for i in range(1, 500)
        }
        self.save_manifest()
        packer.main([str(self.binary), str(self.executable), "--manifest", str(self.manifest)])
        data = self.executable.read_bytes()
        _, manifest_len, _ = packer.MANIFEST_FOOTER.unpack_from(data, len(data) - 12)
        footer_pos = len(data) - 12 - manifest_len - packer.RESOURCE_FOOTER.size
        magic, count, size, crc = packer.RESOURCE_FOOTER.unpack_from(data, footer_pos)
        self.assertEqual((magic, count), (b"LXR2", 499))
        directory = data[footer_pos - size:footer_pos]
        self.assertEqual(zlib.crc32(directory), crc)
        records_len = size - 4 * count
        offsets = struct.unpack_from(f"<{count}I", directory, records_len)
        self.assertEqual(offsets[0], 0)
        self.assertEqual(list(offsets), sorted(offsets))
        for ordinal in (0, 171, 498):
            data_offset, data_len, alias_len = packer.RESOURCE_RECORD.unpack_from(
                directory, offsets[ordinal])
            alias = directory[offsets[ordinal] + 10:offsets[ordinal] + 10 + alias_len]
            self.assertEqual(alias, f":/sprites/p{ordinal + 1:03d}/idle.lxim".encode())
            self.assertEqual((data_len, data[data_offset:data_offset + data_len]), (2, b"\x12\x34"))

    def test_invalid_resource_alias_or_missing_file_rejected(self) -> None:
        for alias, source in [
            (":/../secret", "missing.bin"),
            (":/dir//file", "missing.bin"),
            ("img/image.png", "missing.bin"),
            (":/missing", "missing.bin"),
        ]:
            with self.subTest(alias=alias):
                self.metadata["resources"] = {alias: source}
                self.save_manifest()
                with self.assertRaises(SystemExit) as failure:
                    packer.main([str(self.binary), str(self.executable), "--manifest", str(self.manifest)])
                self.assertEqual(failure.exception.code, 2)

    def test_empty_resource_mapping_preserves_old_manifest_layout(self) -> None:
        self.metadata["resources"] = {}
        self.save_manifest()
        packer.main([str(self.binary), str(self.executable), "--manifest", str(self.manifest)])
        packed = self.executable.read_bytes()
        _, size, _ = struct.unpack_from("<4sII", packed, len(packed) - 12)
        self.assertEqual(len(packed) - 12 - size, packer.HEADER_SIZE + len(self.binary.read_bytes()))
        self.assertTrue(packed[68:68 + size].startswith(b"{"))

    def test_resource_file_directory_conflict_rejected(self) -> None:
        (self.folder / "data.bin").write_bytes(b"hello")
        self.metadata["resources"] = {
            ":/config": "data.bin",
            ":/config/settings.json": "data.bin",
        }
        self.save_manifest()
        with self.assertRaises(SystemExit) as failure:
            packer.main([str(self.binary), str(self.executable), "--manifest", str(self.manifest)])
        self.assertEqual(failure.exception.code, 2)

    def test_resource_name_longer_than_directory_abi_rejected(self) -> None:
        (self.folder / "data.bin").write_bytes(b"hello")
        self.metadata["resources"] = {":/" + "a" * 64: "data.bin"}
        self.save_manifest()
        with self.assertRaises(SystemExit) as failure:
            packer.main([str(self.binary), str(self.executable), "--manifest", str(self.manifest)])
        self.assertEqual(failure.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
