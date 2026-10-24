from __future__ import annotations

import argparse
import sys
import subprocess
from pathlib import Path

# Support both `python -m lexxos_emu.gui` and direct execution by IDEs.
if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    __package__ = "lexxos_emu"

from .sim_build import build_simulator, simulator_library


def find_default_sim_library(repo_root: Path) -> Path | None:
    candidates = tuple(simulator_library(repo_root, profile) for profile in ("minimal", "full"))
    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()
    return None


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Run LexxOS inside the PySide6 GUI emulator")
    parser.add_argument("--lib", help="Use an existing library instead of automatic build")
    parser.add_argument("--profile", choices=("minimal", "full"), default="minimal")
    parser.add_argument("--no-build", action="store_true", help="Use the selected profile's existing library")
    parser.add_argument("--build-only", action="store_true", help="Build without opening the GUI")
    storage = parser.add_mutually_exclusive_group()
    storage.add_argument("--sd-image", help="Optional raw SD/FAT image exposed as /dev/sd0")
    storage.add_argument("--sd-dir", help="Optional host directory imported as /dev/sd0 FAT contents")
    parser.add_argument("--sd-capacity", choices=("256M", "512M", "16G", "32G"), default="16G",
                        help="Capacity for --sd-dir (sparse FAT32 images)")
    parser.add_argument("--sd-fs", choices=("fat32", "fat16"), default="fat32",
                        help="Filesystem for --sd-dir (FAT16 only with 256M/512M)")
    parser.add_argument("--sd-writable", action="store_true",
                        help="Allow changes to a persistent --sd-image (off by default)")
    parser.add_argument("--oled", action="store_true", help="Attach a simulated SSD1306 at I2C address 0x3C")
    parser.add_argument("--audio-mode", choices=("off", "pwm", "adc"), default="pwm",
                        help="Emulated audio device (default: pwm)")
    parser.add_argument("--audio-output", choices=("auto", "on", "off", "wav"), default="auto",
                        help="Host audio output (default: auto, using sounddevice)")
    parser.add_argument("--audio-wav-path", default="lexxos-audio.wav",
                        help="Output WAV filename when --audio-output=wav")

    args = parser.parse_args(argv)

    repo_root = Path(__file__).resolve().parents[2]
    if args.build_only and (args.lib or args.no_build):
        parser.error("--build-only cannot be combined with --lib or --no-build")
    try:
        if args.lib:
            library_path = Path(args.lib).expanduser().resolve()
        elif args.no_build:
            library_path = simulator_library(repo_root, args.profile)
        else:
            library_path = build_simulator(repo_root, args.profile)
        if not library_path.is_file():
            parser.error(f"Simulator library does not exist: {library_path}")
    except (OSError, subprocess.CalledProcessError) as exc:
        print(f"Simulator build failed: {exc}", file=sys.stderr)
        return 1
    if args.build_only:
        return 0
    try:
        from PySide6.QtWidgets import QApplication
        from .gui_window import LexxOsEmulatorWindow
    except ImportError as exc:
        print(f"GUI dependency missing: {exc}. Install emulator/pyproject.toml dependencies.", file=sys.stderr)
        return 1
    sd_image = Path(args.sd_image).expanduser().resolve() if args.sd_image else None
    sd_directory = Path(args.sd_dir).expanduser().resolve() if args.sd_dir else None

    app = QApplication(sys.argv[:1])
    window = LexxOsEmulatorWindow(
        library_path=library_path,
        sd_image=sd_image,
        sd_directory=sd_directory,
        sd_capacity=args.sd_capacity,
        sd_filesystem=args.sd_fs,
        sd_writable=args.sd_writable,
        oled=args.oled,
        audio_mode=args.audio_mode,
        audio_output=args.audio_output,
        audio_wav_path=Path(args.audio_wav_path).expanduser(),
    )
    window.show()
    from PySide6.QtCore import QTimer
    QTimer.singleShot(0, window.boot)
    return app.exec()


__all__ = ["find_default_sim_library", "main"]

if __name__ == "__main__":
    raise SystemExit(main())
