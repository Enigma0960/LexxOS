#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

REQUIRED_PICO_BOARDS = {
    "picocalc_pico": "pico",
    "picocalc_pico2": "pico2",
    "picocalc_pico_w": "pico_w",
    "picocalc_pico2w": "pico2_w",
}


@dataclass(frozen=True)
class Finding:
    preset: str
    message: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Check LexxOS board presets for board-specific SDK settings.")
    parser.add_argument(
        "root",
        nargs="?",
        default=".",
        help="Repository root. Defaults to the current directory.",
    )
    return parser.parse_args()


def load_presets(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict):
        raise ValueError("CMakePresets.json root must be an object")
    return data


def cache_variables(preset: dict[str, Any]) -> dict[str, Any]:
    variables = preset.get("cacheVariables", {})
    if not isinstance(variables, dict):
        return {}
    return variables


def string_value(value: Any) -> str:
    if isinstance(value, dict):
        value = value.get("value", "")
    return str(value)


def check_preset(preset: dict[str, Any]) -> list[Finding]:
    name = str(preset.get("name", "<unnamed>"))
    variables = cache_variables(preset)
    lexxos_board = string_value(variables.get("LEXXOS_BOARD", ""))
    required_pico_board = REQUIRED_PICO_BOARDS.get(lexxos_board)
    if required_pico_board is None:
        return []

    actual_pico_board = string_value(variables.get("PICO_BOARD", ""))
    if actual_pico_board == required_pico_board:
        return []

    return [
        Finding(
            preset=name,
            message=(
                f"LEXXOS_BOARD={lexxos_board} requires PICO_BOARD={required_pico_board}, "
                f"got {actual_pico_board or '<unset>'}"
            ),
        )
    ]


def main() -> int:
    args = parse_args()
    root = Path(args.root).resolve()
    data = load_presets(root / "CMakePresets.json")
    presets = data.get("configurePresets", [])
    if not isinstance(presets, list):
        print("CMakePresets.json: configurePresets must be a list", file=sys.stderr)
        return 1

    by_name = {preset["name"]: preset for preset in presets if isinstance(preset, dict)}

    def resolved(preset, stack=()):
        name = preset["name"]
        if name in stack:
            raise ValueError(f"Preset inheritance cycle: {stack + (name,)}")
        parents = preset.get("inherits", [])
        if isinstance(parents, str):
            parents = [parents]
        variables = {}
        for parent in reversed(parents):
            variables.update(resolved(by_name[parent], stack + (name,))["cacheVariables"])
        variables.update(cache_variables(preset))
        return {**preset, "cacheVariables": variables}

    findings: list[Finding] = []
    for preset in presets:
        if isinstance(preset, dict):
            findings.extend(check_preset(resolved(preset)))

    board_source = (root / "cmake" / "Boards.cmake").read_text(encoding="utf-8")
    board_match = re.search(r"set\(_LEXXOS_BOARD_IDS\s+(.*?)\)", board_source, re.DOTALL)
    if board_match is None:
        raise ValueError("Cannot find board registry in cmake/Boards.cmake")
    builds = {preset["name"]: preset for preset in data.get("buildPresets", [])}
    for board in board_match.group(1).split():
        for flavor in ("minimal", "full"):
            name = f"{flavor}-{board}"
            if name not in by_name:
                findings.append(Finding(name, "Missing public configure preset"))
                continue
            variables = resolved(by_name[name])["cacheVariables"]
            required = {"LEXXOS_BOARD": board, "LEXXOS_BUILD_FLAVOR": flavor,
                        "LEXXOS_FEATURE_DISPLAY": "ON", "LEXXOS_FEATURE_KEYBOARD": "ON",
                        "LEXXOS_FEATURE_SD_CARD": "ON", "LEXXOS_FEATURE_FATFS": "ON",
                        "LEXXOS_FEATURE_DEBUG_DISPLAY": "OFF",
                        "LEXXOS_COMMANDS_TESTS": "ON" if flavor == "full" else "OFF",
                        "LEXXOS_COMMANDS_EXPERIMENTAL": "ON" if flavor == "full" else "OFF"}
            if flavor == "minimal":
                required.update({f"LEXXOS_FEATURE_{feature}": "OFF" for feature in
                                 ("GAMEBOY", "MIDI", "HEALTH", "GPIO_ANALYZER", "TEST_COMMANDS")})
            else:
                required.update({"LEXXOS_FEATURE_HEALTH": "ON", "LEXXOS_FEATURE_TEST_COMMANDS": "ON"})
                for feature in ("GAMEBOY", "MIDI"):
                    required[f"LEXXOS_FEATURE_{feature}"] = "OFF" if board in {"picocalc_pico", "picocalc_pico_w"} else "ON"
            for key, expected in required.items():
                if string_value(variables.get(key, "")) != expected:
                    findings.append(Finding(name, f"{key} must be {expected}"))
            if builds.get(name, {}).get("configurePreset") != name:
                findings.append(Finding(name, "Missing matching public build preset"))

    if findings:
        for finding in findings:
            print(f"CMakePresets.json:{finding.preset}: {finding.message}")
        return 1

    print("Board preset check passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
