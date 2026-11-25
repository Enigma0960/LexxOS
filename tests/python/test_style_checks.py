from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))

import check_cpp_layout
import check_markdown


class StyleChecksTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def write(self, name, content):
        path = self.root / name
        path.write_text(content, encoding="utf-8")
        return path

    def test_independent_classes_are_reported(self):
        path = self.write("types.h", "namespace sample {\nclass First {};\nclass Second final {};\n}\n")
        findings = check_cpp_layout.check_header_classes(path, self.root)
        self.assertEqual(len(findings), 1)
        self.assertEqual(findings[0].line, 3)
        self.assertIn("First, Second", findings[0].message)

    def test_forward_declarations_nested_classes_and_templates_are_allowed(self):
        path = self.write("queue.h", """class Event;
// class Ignored {};
template <typename T>
class Queue {
    class Guard {};
    T* data;
};
""")
        self.assertEqual(check_cpp_layout.check_header_classes(path, self.root), [])

    def test_using_declaration_does_not_hide_namespace_directive(self):
        path = self.write("main.cpp", "using sample::Type;\nusing namespace sample;\n")
        findings = check_cpp_layout.check_file(path, self.root, False, False)
        self.assertEqual([finding.line for finding in findings], [2])

    def test_markdown_ignores_code_but_reports_real_broken_links(self):
        path = self.write("example.md", """```cpp
[](void* context) { return context; }
```
Inline `[](void*)` and ``[example](not-a-link)``.
[Missing](missing.md)
""")
        findings = check_markdown.check_file(path, self.root)
        self.assertEqual([finding.line for finding in findings], [5])
        self.assertIn("missing.md", findings[0].message)

    def test_short_fence_does_not_close_long_fence(self):
        path = self.write("example.md", "````markdown\n```\n[example](missing.md)\n````\n")
        self.assertEqual(check_markdown.check_file(path, self.root), [])
        path.write_text("````markdown\n```\n", encoding="utf-8")
        findings = check_markdown.check_file(path, self.root)
        self.assertEqual(len(findings), 1)
        self.assertIn("unclosed", findings[0].message)


if __name__ == "__main__":
    unittest.main()
