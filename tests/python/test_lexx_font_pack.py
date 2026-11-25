"""Host-only tests of the FreeType bitmap header generator API."""
import importlib.util
from pathlib import Path
import unittest

SCRIPT = Path(__file__).resolve().parents[2] / "tools" / "lexx_font_pack.py"
SPEC = importlib.util.spec_from_file_location("lexx_font_pack", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
module = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(module)


class FontPackerTests(unittest.TestCase):
    def test_ranges(self):
        self.assertEqual(module.codepoints('U+0041-U+0042,U+0041,U+0401'), [65, 66, 1025])

    def test_invalid_range(self):
        with self.assertRaises(ValueError):
            module.codepoints('U+0042-U+0041')

    def test_header_contract(self):
        result = module.header([(65, [0x1, 0x2])], 9, 2, 'defaultGlyphs', 'Test.ttf', 'OFL')
        self.assertIn('std::array<Glyph, 1>', result)
        self.assertIn('0x0041U', result)
        self.assertIn('0x0002U', result)


if __name__ == '__main__':
    unittest.main()
