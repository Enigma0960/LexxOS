#!/usr/bin/env python3
"""Select built-ins once for both compilation and registration."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

GROUPS = {"SYSTEM", "UTILITIES", "ADMIN", "DIAGNOSTICS", "APPLICATIONS", "TESTS", "EXPERIMENTAL"}

REQUIREMENTS = {
    "AUDIO", "BACKLIGHT", "CARDPUTER_ADV", "DESKTOP", "DISPLAY", "DISPLAY_TEST_COMMANDS",
    "ESPIDF", "ESP_SPI", "FATFS", "FULL", "GAMEBOY", "GB_RUNTIME", "GPIO_ANALYZER",
    "HOSTED", "I2C", "JC", "KEYBOARD_TEST_COMMANDS", "LCD_CONSOLE", "LED", "MIC",
    "MIDI", "NET", "PICO", "PICOCALC", "PORTABLE_BEEP", "PSRAM", "SD_CARD",
    "SD_CARD_TEST_COMMANDS", "SD_SPI", "WIFI",
}


def capabilities(board: str, platform: str, flavor: str, backend: str, enabled: set[str]) -> set[str]:
    result = set(enabled)
    if platform == "host":
        result.discard("DISPLAY")
        result.discard("DISPLAY_TEST_COMMANDS")
        result.discard("KEYBOARD_TEST_COMMANDS")
    def add(name: str, condition: bool) -> None:
        if condition:
            result.add(name)
    picocalc = board in {"picocalc_pico_w", "picocalc_pico2w"}
    jc = board == "jc_esp32p4_m3_dev"
    adv = board == "m5stack_cardputer_adv"
    cardputer = adv or board == "m5stack_cardputer"
    pico_wifi = platform == "pico" and (picocalc or board == "dev_pico2w") and "CYW43" in enabled
    add("PICOCALC", picocalc)
    add("JC", jc)
    add("CARDPUTER_ADV", adv)
    add("PICO", platform == "pico")
    add("ESPIDF", platform == "espidf")
    add("FULL", flavor == "full")
    add("DESKTOP", platform in {"host", "sim"})
    add("SD_SPI", backend == "spi")
    add("NET", platform == "espidf" or pico_wifi)
    add("WIFI", (platform == "espidf" and (not jc or "HOSTED" in enabled))
        or pico_wifi)
    add("AUDIO", platform == "sim" or picocalc or jc or adv)
    add("MIC", jc or adv)
    add("PORTABLE_BEEP", platform in {"sim", "espidf"})
    add("LCD_CONSOLE", "DISPLAY" in enabled and (picocalc or cardputer or jc))
    add("BACKLIGHT", "DISPLAY" in enabled and (picocalc or platform == "espidf"))
    add("LED", board in {"dev_pico", "dev_pico_sdio", "dev_pico2w", "euse_m4_demo_stm32f407", "dev-sim"})
    add("I2C", platform != "host")
    add("ESP_SPI", platform == "espidf" and not jc)
    add("GB_RUNTIME", "PSRAM" in enabled or platform in {"host", "sim", "espidf"})
    return result


def select(entries: list[dict], enabled: set[str]) -> list[dict]:
    selected = []
    names = set()
    for entry in entries:
        if entry["group"] not in GROUPS:
            raise ValueError(f"Unknown group for {entry['name']}")
        unknown = set(entry["requires"]) - REQUIREMENTS
        if unknown:
            raise ValueError(f"Unknown dependencies for {entry['name']}: {sorted(unknown)}")
        if "GROUP_" + entry["group"] not in enabled or not set(entry["requires"]) <= enabled:
            continue
        if entry["name"] in names:
            raise ValueError(f"Duplicate command: {entry['name']}")
        names.add(entry["name"])
        selected.append(entry)
    return selected


def generate(entries: list[dict], selected: list[dict], output: Path, enabled: set[str]) -> None:
    output.mkdir(parents=True, exist_ok=True)
    names = {e["name"] for e in selected}
    sources = {s for e in selected for s in e["sources"]}
    excluded = {s for e in entries for s in e["sources"]} - sources
    includes = sorted({e["header"] for e in selected})
    registration = '#include <array>\n#include <lexxos/apps/programs.h>\n'
    registration += ''.join(f'#include <{h}>\n' for h in includes)
    registration += '\nvoid lexxos::apps::registerBuiltinPrograms(lexxos::program::ProgramManager& programs) {\n'
    # One call site reuses descriptor storage even in debug builds. Separate
    # expressions can reserve one temporary per command and overflow MCU stacks.
    registration += f'\tstatic constexpr std::array<lexxos::program::ProgramDescriptor (*)(), {len(selected)}> factories{{{{\n'
    registration += ''.join(f'\t\t&{e["type"]}::descriptor,\n' for e in selected)
    registration += '\t}};\n\tfor (auto factory : factories) {\n\t\tprograms.registerProgram(factory());\n\t}\n'
    registration += '}\n'
    (output / 'builtin_commands.inc').write_text(registration)
    macros = '#pragma once\n\n'
    for name in sorted({e['name'] for e in entries}):
        macros += f'#define LEXXOS_COMMAND_{name.upper().replace("-", "_")} {int(name in names)}\n'
    macros += f'\n#define LEXXOS_COMMAND_TEST_OPERATIONS {int("GROUP_TESTS" in enabled)}\n'
    (output / 'command_selection.h').write_text(macros)
    cmake = 'set(LEXXOS_EXCLUDED_COMMAND_SOURCES\n' + ''.join(f'    "{s}"\n' for s in sorted(excluded)) + ')\n'
    cmake += 'set(LEXXOS_SELECTED_COMMAND_SOURCES\n' + ''.join(f'    "{s}"\n' for s in sorted(sources)) + ')\n'
    cmake += f'set(LEXXOS_COMMAND_HAS_GAMEBOY {int(any("GAMEBOY" in e["requires"] for e in selected))})\n'
    cmake += f'set(LEXXOS_COMMAND_HAS_GBEMU {int("gbemu" in names)})\n'
    (output / 'command_sources.cmake').write_text(cmake)
    report = [{"name": e['name'], "group": e['group'], "enabled": e in selected,
               "missing": sorted((set(e['requires']) | {'GROUP_' + e['group']}) - enabled)} for e in entries]
    (output / 'commands.json').write_text(json.dumps(report, indent=2) + '\n')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    for key in ('board', 'platform', 'flavor', 'backend'):
        parser.add_argument('--' + key, required=True)
    parser.add_argument('--enabled', action='append', default=[])
    args = parser.parse_args()
    enabled = capabilities(args.board, args.platform, args.flavor, args.backend, set(args.enabled))
    entries = json.loads(args.manifest.read_text())
    generate(entries, select(entries, enabled), args.output, enabled)


if __name__ == '__main__':
    main()
