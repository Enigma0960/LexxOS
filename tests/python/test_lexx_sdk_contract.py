"""Validate opt-in manifest contracts against the authoritative .lexx header."""

from __future__ import annotations

import importlib.util
import json
import pathlib
import tempfile
import unittest

PACKER_PATH = pathlib.Path(__file__).resolve().parents[2] / "tools" / "lexx_pack.py"
SPEC = importlib.util.spec_from_file_location("lexx_pack_sdk_contract", PACKER_PATH)
assert SPEC is not None and SPEC.loader is not None
packer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(packer)


class ManifestContractTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.manifest = pathlib.Path(self.temp.name) / "manifest.json"
        self.data = {
            "schema": 1,
            "id": "org.lexxos.tests.contract",
            "name": "Test",
            "version": "0.1.0",
            "author": "LexxOS",
            "license": "Apache-2.0",
            "capabilities": ["console", "graphics", "clock"],
            "memory": {"guest_bytes": 65536, "stack_bytes": 8192},
        }

    def pack(self, capabilities: int = (1 | 128 | 256),
             memory_size: int = 65536, stack_size: int = 8192) -> bytes:
        self.manifest.write_text(json.dumps(self.data), encoding="utf-8")
        return packer.pack_manifest(
            self.manifest, capabilities=capabilities,
            memory_size=memory_size, stack_size=stack_size)

    def test_matching_contract_is_accepted(self) -> None:
        self.assertIn(b"LXMF", self.pack())

    def test_capability_mismatch_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "capabilities does not match"):
            self.pack(capabilities=1)

    def test_memory_mismatch_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "memory does not match"):
            self.pack(memory_size=32768)
        with self.assertRaisesRegex(ValueError, "memory does not match"):
            self.pack(stack_size=4096)

    def test_unknown_or_duplicate_rights_rejected(self) -> None:
        self.data["capabilities"] = ["console", "graphics", "graphics"]
        with self.assertRaisesRegex(ValueError, "duplicates"):
            self.pack()
        self.data["capabilities"] = ["console", "root"]
        with self.assertRaisesRegex(ValueError, "known names"):
            self.pack()

    def test_legacy_manifest_still_supported(self) -> None:
        del self.data["capabilities"]
        del self.data["memory"]
        self.assertIn(b"LXMF", self.pack())


if __name__ == "__main__":
    unittest.main()
