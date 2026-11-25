#!/usr/bin/env python3
"""Проверка фактического состава библиотеки CE по compile_commands.json."""
import argparse
import json
from pathlib import Path


def check(build: Path, archive: Path, portable: bool = False) -> None:
    commands = json.loads((build / "compile_commands.json").read_text())
    files = [Path(entry["file"]).as_posix() for entry in commands]
    forbidden = ("/lexxos/shell/", "/lexxos/vfs/", "/lexxos/program/",
                 "/lexxos/external_program/", "/lexxos/gameboy/", "/lexxos/apps/",
                 "/lexxos/app/", "/lexxos/ui/", "/components/lexxos/",
                 "/ce/examples/", "/lexxos/platform/espidf/board_")
    for file in files:
        if any(part in file for part in forbidden):
            raise ValueError(f"В библиотеку CE попал лишний исходник: {file}")
    required = ("/lexxos/core/core_system.cpp", "/lexxos/core/bootstrap.cpp")
    if not portable:
        required += ("/lexxos/kernel/espidf/scheduler_espidf.cpp",
                     "/lexxos/platform/espidf/platform_espidf_ce.cpp")
    for suffix in required:
        if not any(file.endswith(suffix) for file in files):
            raise ValueError(f"Отсутствует обязательный исходник CE: {suffix}")
    for entry in commands:
        if "/src/lexxos/" in Path(entry["file"]).as_posix():
            command = entry.get("command", " ".join(entry.get("arguments", [])))
            for definition in (("LEXXOS_EDITION_CE=1",) if portable else ("LEXXOS_EDITION_CE=1", "LEXXOS_PLATFORM_ESPIDF=1")):
                if definition not in command:
                    raise ValueError(f"Отсутствует общий ABI-макрос {definition}: {entry['file']}")
    if not archive.is_file():
        raise ValueError("Сначала соберите библиотеку CE")
    print("CE: состав исходников и ABI проверены; shell/VFS/ProgramManager/guest runtime отсутствуют")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("archive", type=Path)
    args = parser.parse_args()
    try:
        check(args.build, args.archive)
    except (ValueError, OSError) as error:
        parser.exit(1, f"CE build check failed: {error}\n")
