#!/usr/bin/env python3

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from urllib.parse import unquote

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
    "managed_components",
    "third_party",
    "node_modules",
    "venv",
}

FENCE_RE = re.compile(r"^\s*(```+|~~~+)")
LINK_RE = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
REFERENCE_LINK_RE = re.compile(r"^\s*\[[^\]]+\]:\s*(\S+)")


@dataclass(frozen=True)
class Finding:
    path: Path
    line: int
    message: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Check LexxOS Markdown files.")
    parser.add_argument(
        "root",
        nargs="?",
        default=".",
        help="Repository root. Defaults to the current directory.",
    )
    return parser.parse_args()


def should_skip(path: Path, root: Path) -> bool:
    try:
        relative = path.relative_to(root)
    except ValueError:
        return True

    return any(part in IGNORED_DIR_NAMES or part.startswith("cmake-build-") for part in relative.parts)


def markdown_files(root: Path) -> list[Path]:
    return sorted(
        path
        for path in root.rglob("*.md")
        if path.is_file() and not should_skip(path, root)
    )


def is_external_link(target: str) -> bool:
    lowered = target.lower()
    return (
            lowered.startswith("http://")
            or lowered.startswith("https://")
            or lowered.startswith("mailto:")
            or lowered.startswith("tel:")
    )


def normalize_target(raw_target: str) -> str:
    target = raw_target.strip()
    if target.startswith("<") and target.endswith(">"):
        target = target[1:-1].strip()

    if " " in target:
        target = target.split(" ", 1)[0]

    return unquote(target)


def check_fences(path: Path, relative: Path, lines: list[str]) -> list[Finding]:
    findings: list[Finding] = []
    open_fence: str | None = None
    open_line = 0

    for line_number, line in enumerate(lines, start=1):
        match = FENCE_RE.match(line)
        if not match:
            continue

        marker = match.group(1)
        marker_family = marker[0]

        if open_fence is None:
            open_fence = marker_family
            open_line = line_number
        elif open_fence == marker_family:
            open_fence = None
            open_line = 0

    if open_fence is not None:
        findings.append(Finding(relative, open_line, "unclosed fenced code block"))

    return findings


def target_exists(path: Path, target: str) -> bool:
    if target.startswith("#") or not target:
        return True
    if is_external_link(target):
        return True

    target_path = target.split("#", 1)[0]
    if not target_path:
        return True
    if ":" in target_path and not target_path.startswith("./") and not target_path.startswith("../"):
        return True

    resolved = (path.parent / target_path).resolve()
    return resolved.exists()


def check_links(path: Path, relative: Path, lines: list[str]) -> list[Finding]:
    findings: list[Finding] = []

    for line_number, line in enumerate(lines, start=1):
        targets = [match.group(1) for match in LINK_RE.finditer(line)]

        reference_match = REFERENCE_LINK_RE.match(line)
        if reference_match:
            targets.append(reference_match.group(1))

        for raw_target in targets:
            target = normalize_target(raw_target)
            if not target_exists(path, target):
                findings.append(
                    Finding(
                        relative,
                        line_number,
                        f"broken local Markdown link: {target}",
                    )
                )

    return findings


def check_file(path: Path, root: Path) -> list[Finding]:
    relative = path.relative_to(root)
    lines = path.read_text(encoding="utf-8").splitlines()

    findings: list[Finding] = []
    findings.extend(check_fences(path, relative, lines))
    findings.extend(check_links(path, relative, lines))
    return findings


def main() -> int:
    args = parse_args()
    root = Path(args.root).resolve()
    findings: list[Finding] = []

    for path in markdown_files(root):
        findings.extend(check_file(path, root))

    if findings:
        for finding in findings:
            print(f"{finding.path}:{finding.line}: {finding.message}")
        return 1

    print("Markdown check passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
