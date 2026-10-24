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


if __name__ == "__main__":
    unittest.main()
