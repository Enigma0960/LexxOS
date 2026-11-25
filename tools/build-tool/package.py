# !/usr/bin/env python3

from __future__ import annotations

import argparse
import platform
import shutil
import subprocess
import sys
from pathlib import Path


def repository_root() -> Path:
    path = Path(__file__).resolve()
    for candidate in [path.parent, *path.parents]:
        if (candidate / "CMakeLists.txt").is_file() and (candidate / "CMakePresets.json").is_file():
            return candidate
    raise SystemExit(f"lexxos-build-tool package: repository root was not found near {path}")


def default_output_path(root: Path, name: str) -> Path:
    suffix = ".exe" if platform.system() == "Windows" else ""
    return root / "tools" / f"{name}{suffix}"


def parse_arguments(root: Path) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        prog="tools/build-tool/package.py",
        description="Package LexxOS Build Tool GUI as a standalone executable.",
    )
    parser.add_argument(
        "--name",
        default="lexxos-build-tool",
        help="Executable name passed to PyInstaller. Default: lexxos-build-tool.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="Final executable path. Default: tools/<name>.",
    )
    parser.add_argument(
        "--workpath",
        type=Path,
        default=root / "build" / "build-tool" / "pyinstaller-work",
        help="PyInstaller work directory.",
    )
    parser.add_argument(
        "--distpath",
        type=Path,
        default=root / "build" / "build-tool" / "dist",
        help="PyInstaller dist directory.",
    )
    parser.add_argument(
        "--specpath",
        type=Path,
        default=root / "build" / "build-tool" / "spec",
        help="PyInstaller spec directory.",
    )
    parser.add_argument("--python", default=sys.executable, help="Python executable used to run PyInstaller.")
    parser.add_argument("--console", action="store_true", help="Build a console app instead of a windowed GUI app.")
    parser.add_argument("--onedir", action="store_true", help="Build an onedir distribution instead of onefile.")
    parser.add_argument("--clean", action="store_true", help="Remove previous PyInstaller build directories first.")
    parser.add_argument("--dry-run", action="store_true", help="Print commands without running them.")
    return parser.parse_args()


def remove_previous_outputs(args: argparse.Namespace) -> None:
    for path in [args.workpath, args.distpath, args.specpath]:
        if path.exists():
            print(f"remove {path}")
            shutil.rmtree(path)


def run(command: list[str], cwd: Path, dry_run: bool) -> None:
    print("+", " ".join(command))
    if dry_run:
        return
    subprocess.run(command, cwd=cwd, check=True)


def pyinstaller_command(root: Path, args: argparse.Namespace) -> list[str]:
    entrypoint = root / "tools" / "build-tool" / "lexxos_build_tool" / "main.py"
    command = [
        args.python,
        "-m",
        "PyInstaller",
        "--noconfirm",
        "--name",
        args.name,
        "--workpath",
        str(args.workpath),
        "--distpath",
        str(args.distpath),
        "--specpath",
        str(args.specpath),
    ]
    if not args.onedir:
        command.append("--onefile")
    if args.console:
        command.append("--console")
    else:
        command.append("--windowed")
    command.append(str(entrypoint))
    return command


def built_executable(args: argparse.Namespace) -> Path:
    suffix = ".exe" if platform.system() == "Windows" else ""
    if args.onedir:
        return args.distpath / args.name / f"{args.name}{suffix}"
    return args.distpath / f"{args.name}{suffix}"


def copy_result(root: Path, args: argparse.Namespace) -> None:
    source = built_executable(args)
    output = args.output or default_output_path(root, args.name)
    if not source.exists():
        raise SystemExit(f"lexxos-build-tool package: expected artifact was not produced: {source}")
    output.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, output)
    if platform.system() != "Windows":
        output.chmod(output.stat().st_mode | 0o111)
    print(f"wrote {output}")


def main() -> int:
    root = repository_root()
    args = parse_arguments(root)
    if args.output is None:
        args.output = default_output_path(root, args.name)
    else:
        args.output = args.output.expanduser()
        if not args.output.is_absolute():
            args.output = root / args.output

    if args.clean:
        remove_previous_outputs(args)
    run(pyinstaller_command(root, args), root, args.dry_run)
    if not args.dry_run:
        copy_result(root, args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
