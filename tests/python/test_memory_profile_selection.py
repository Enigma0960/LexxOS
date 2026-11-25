"""Reject incompatible target memory compositions before emitting a header."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class MemoryProfileSelectionTests(unittest.TestCase):
    def reject(self, arguments, reason):
        with tempfile.TemporaryDirectory() as directory:
            script = Path(directory) / "profile.cmake"
            script.write_text(
                f'include("{ROOT / "config/cmake/MemoryProfiles.cmake"}")\n'
                f'lexxos_attach_memory_profile(invalid {arguments})\n',
                encoding="utf-8",
            )
            result = subprocess.run(
                ["cmake", "-P", str(script)], text=True, capture_output=True, check=False
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(reason, result.stderr)

    def test_incompatible_chip_platform(self):
        self.reject("CHIP rp2040 PLATFORM rk3506", "Incompatible memory chip/platform")

    def test_unknown_chip(self):
        self.reject("CHIP unknown PLATFORM pico", "Incompatible memory chip/platform")

    def test_incompatible_board(self):
        self.reject("CHIP rp2040 PLATFORM pico BOARD picocalc_pico2w", "Incompatible memory board")

    def test_unknown_board(self):
        self.reject("CHIP esp32s3 PLATFORM espidf BOARD unknown", "Incompatible memory board")

    def test_missing_value(self):
        self.reject("CHIP rp2040 PLATFORM", "Invalid memory profile arguments")


if __name__ == "__main__":
    unittest.main()
