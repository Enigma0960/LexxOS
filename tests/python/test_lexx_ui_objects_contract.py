"""Header dependency rules for the object-oriented guest UI SDK."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2] / "apps" / "sdk" / "lexx"
NAMES = (
    "label", "button", "checkbox", "text_field", "list_view",
    "slider", "progress_bar",
)


class UiObjectHeaderTests(unittest.TestCase):
    def test_independent_control_headers(self):
        for name in NAMES:
            with self.subTest(name=name):
                content = (ROOT / "ui" / f"{name}.h").read_text()
                self.assertIn("#include <lexx/ui/widget.h>", content)
                for other in NAMES:
                    if other != name:
                        self.assertNotIn(f"#include <lexx/ui/{other}.h>", content)

    def test_window_is_widget_type_agnostic(self):
        content = (ROOT / "ui" / "window.h").read_text()
        self.assertIn("#include <lexx/ui/widget.h>", content)
        for name in NAMES:
            self.assertNotIn(f"#include <lexx/ui/{name}.h>", content)

    def test_protocol_is_unchanged(self):
        content = (ROOT / "ui" / "core.h").read_text()
        for assertion in (
                "sizeof(Control) == 48", "sizeof(Document) == 420",
                "sizeof(Event) == 40", "sizeof(ListItems) == 260",
        ):
            self.assertIn(assertion, content)
        self.assertIn("operation_show = 6", content)
        self.assertIn("operation_set_items = 10", content)
        self.assertNotIn("std::function", content)


if __name__ == "__main__":
    unittest.main()
