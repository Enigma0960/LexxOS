#!/usr/bin/env python3

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

IMPLEMENTATION_SUFFIXES = {".cpp", ".cc", ".cxx"}
DEFAULT_SCAN_ROOTS = (
    "src",
    "emulator/native",
    "tests/cpp",
)
IGNORED_DIR_NAMES = {
    ".cache",
    ".git",
    ".idea",
    ".mypy_cache",
    ".pytest_cache",
    ".venv",
    ".vscode",
    "__pycache__",
    "_deps",
    "build",
    "cmake-build-debug",
    "cmake-build-release",
    "dist",
    "external",
    "third_party",
    "node_modules",
    "venv",
}

USING_NAMESPACE_RE = re.compile(r"(^|;)\s*using\s+namespace\s+[A-Za-z_][A-Za-z0-9_:]*\s*;")
TYPE_DECL_RE = re.compile(r"^\s*(class|struct)\s+[A-Za-z_][A-Za-z0-9_]*(\s*[:{]|\s*$)")


@dataclass(frozen=True)
class Finding:
    path: Path
    line: int
    message: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Check LexxOS C++ implementation-file layout rules.",
    )
    parser.add_argument(
        "root",
        nargs="?",
        default=".",
        help="Repository root. Defaults to the current directory.",
    )
    parser.add_argument(
        "--scan-root",
        action="append",
        default=[],
        help=(
            "Repository-relative directory to scan. Can be passed more than once. "
            "Defaults to src, emulator/native and tests/cpp."
        ),
    )
    parser.add_argument(
        "--allow-using-namespace",
        action="store_true",
        help="Do not report using namespace directives.",
    )
    parser.add_argument(
        "--forbid-cpp-types",
        action="store_true",
        help=(
            "Report class/struct declarations in implementation files. "
            "This is a strict audit mode; the default policy allows implementation-local helper types."
        ),
    )
    parser.add_argument(
        "--allow-cpp-types",
        action="store_true",
        help=argparse.SUPPRESS,
    )
    return parser.parse_args()


def should_skip(path: Path, root: Path) -> bool:
    try:
        relative = path.relative_to(root)
    except ValueError:
        return True

    return any(part in IGNORED_DIR_NAMES or part.startswith("cmake-build-") for part in relative.parts)


def scan_roots(root: Path, requested_scan_roots: list[str]) -> list[Path]:
    selected_roots = requested_scan_roots if requested_scan_roots else list(DEFAULT_SCAN_ROOTS)
    result: list[Path] = []

    for relative_root in selected_roots:
        path = (root / relative_root).resolve()
        if path.exists():
            result.append(path)

    return result


def implementation_files(root: Path, source_root: Path) -> Iterable[Path]:
    if source_root.is_file():
        if source_root.suffix in IMPLEMENTATION_SUFFIXES and not should_skip(source_root, root):
            yield source_root
        return

    for path in source_root.rglob("*"):
        if path.is_file() and path.suffix in IMPLEMENTATION_SUFFIXES and not should_skip(path, root):
            yield path


def strip_comments_and_strings(line: str, in_block_comment: bool) -> tuple[str, bool]:
    result: list[str] = []
    index = 0
    in_string: str | None = None
    escaped = False

    while index < len(line):
        char = line[index]
        next_char = line[index + 1] if index + 1 < len(line) else ""

        if in_block_comment:
            if char == "*" and next_char == "/":
                in_block_comment = False
                index += 2
            else:
                index += 1
            continue

        if in_string is not None:
            result.append(" ")
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == in_string:
                in_string = None
            index += 1
            continue

        if char == "/" and next_char == "/":
            break
        if char == "/" and next_char == "*":
            in_block_comment = True
            index += 2
            continue
        if char == '"' or char == "'":
            in_string = char
            result.append(" ")
            index += 1
            continue

        result.append(char)
        index += 1

    return "".join(result), in_block_comment


def statement_opens_type(clean_line: str, pending_type_declaration: bool) -> bool:
    stripped = clean_line.strip()
    if not stripped:
        return pending_type_declaration

    if pending_type_declaration:
        return "{" not in stripped and ";" not in stripped

    if not TYPE_DECL_RE.search(clean_line):
        return False

    if ";" in stripped and "{" not in stripped:
        return False

    return "{" not in stripped


def check_file(path: Path, root: Path, allow_using_namespace: bool, forbid_cpp_types: bool) -> list[Finding]:
    findings: list[Finding] = []
    in_block_comment = False
    pending_type_declaration = False

    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except UnicodeDecodeError:
        lines = path.read_text(encoding="latin-1").splitlines()

    for line_number, line in enumerate(lines, start=1):
        clean_line, in_block_comment = strip_comments_and_strings(line, in_block_comment)
        relative = path.relative_to(root)

        if not allow_using_namespace and USING_NAMESPACE_RE.search(clean_line):
            findings.append(
                Finding(
                    path=relative,
                    line=line_number,
                    message="using namespace directive in implementation file",
                )
            )

        if forbid_cpp_types:
            stripped = clean_line.strip()
            if pending_type_declaration and "{" in stripped:
                findings.append(
                    Finding(
                        path=relative,
                        line=line_number,
                        message="class/struct declaration continues into implementation file",
                    )
                )
                pending_type_declaration = False
            elif TYPE_DECL_RE.search(clean_line) and not (";" in stripped and "{" not in stripped):
                findings.append(
                    Finding(
                        path=relative,
                        line=line_number,
                        message="class/struct declaration in implementation file",
                    )
                )
            else:
                pending_type_declaration = statement_opens_type(clean_line, pending_type_declaration)

    return findings


def main() -> int:
    args = parse_args()
    root = Path(args.root).resolve()
    findings: list[Finding] = []

    for source_root in scan_roots(root, args.scan_root):
        for path in implementation_files(root, source_root):
            findings.extend(
                check_file(
                    path=path,
                    root=root,
                    allow_using_namespace=args.allow_using_namespace,
                    forbid_cpp_types=args.forbid_cpp_types,
                )
            )

    if findings:
        for finding in findings:
            print(f"{finding.path}:{finding.line}: {finding.message}")
        return 1

    print("C++ layout check passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
