#!/usr/bin/env python3
"""Лексический gate мигрированных границ; не заменяет AST-аудит и runtime-тесты."""

from __future__ import annotations

import argparse
import re
from pathlib import Path

INPUT_CALL = re.compile(
    r"\b(?:program_input::(?:publish|reset|pollEvent|latestEvent|sequence|setInputMode|inputMode|releaseInputMode)"
    r"|console_input::(?:readChar|readAvailable|inject|clearInjected|setMode|mode|pendingInjected))\s*\("
)
VFS_CALL = re.compile(r"\bvfs::(?:stat|open|writeText|sync|createDirectory)\s*\(")
APP_INCLUDE = re.compile(r"^\s*#\s*include\s*[<\"]lexxos/(?:app|apps|os|program)/")
INPUT_ADAPTERS = {"app/program_input.cpp", "app/console_input.cpp"}


def check(root: Path) -> list[str]:
    findings = []
    source = root / "src/lexxos"
    for path in sorted(source.rglob("*.cpp")):
        relative = path.relative_to(source).as_posix()
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            # Проверяются выражения в коде, а не поясняющие комментарии.
            code = line.split("//", 1)[0]
            message = None
            if relative not in INPUT_ADAPTERS and INPUT_CALL.search(code):
                message = "global input call: obtain input service from OS/AppContext"
            elif relative == "apps/gameboy/detail/gameboy_settings.cpp" and VFS_CALL.search(code):
                message = "settings must use the injected Vfs instance"
            elif relative == "platform/rockchip/rk3506/sdmmc_probe.cpp" and APP_INCLUDE.search(code):
                message = "SDMMC backend must not depend on application/CLI layers"
            if message:
                findings.append(f"{path.relative_to(root)}:{number}: {message}")
    # These shared entry points must not regain target-specific selection.
    neutral_files = {
        "composition/board_device_providers.cpp": r"\bLEXXOS_PLATFORM_",
        "runtime/runtime_profile.h": r"(?:#\s*include\s*[<\"](?:memory/|lexxos/platform/)|\bLEXXOS_MEMORY_)",
        "runtime/selected_runtime_profile.h": r"#\s*include\s*[<\"]memory/(?:chips|platforms|boards)/",
    }
    for relative, pattern in neutral_files.items():
        path = source / relative
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if re.search(pattern, line.split("//", 1)[0]):
                findings.append(f"{path.relative_to(root)}:{number}: target selection belongs in build composition")
    return findings


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", nargs="?", default=".", type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    if not (root / "src/lexxos").is_dir():
        parser.error("repository root must contain src/lexxos")
    findings = check(root)
    for finding in findings:
        print(finding)
    if not findings:
        print("Runtime boundaries: OK")
    return int(bool(findings))


if __name__ == "__main__":
    raise SystemExit(main())
