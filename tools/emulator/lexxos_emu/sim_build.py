"""Build the selected public simulator preset before starting the GUI."""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path


def simulator_library(root: Path, profile: str) -> Path:
    if profile not in ("minimal", "full"):
        raise ValueError(f"Unknown simulator profile: {profile}")
    filename = "liblexxos_sim.dylib" if sys.platform == "darwin" else "lexxos_sim.dll" if sys.platform == "win32" else "liblexxos_sim.so"
    return root / "build" / f"{profile}-dev-sim" / "src" / filename


def build_simulator(root: Path, profile: str) -> Path:
    preset = f"{profile}-dev-sim"
    library = simulator_library(root, profile)
    cmake = os.environ.get("LEXXOS_CMAKE_BIN", "cmake")
    for command in ([cmake, "--preset", preset], [cmake, "--build", "--preset", preset, "--parallel", "2"]):
        print("Building simulator: " + " ".join(command), flush=True)
        subprocess.run(command, cwd=root, check=True)
    if not library.is_file():
        raise FileNotFoundError(f"Build completed but simulator library is missing: {library}")
    return library.resolve()
