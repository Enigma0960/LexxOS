from __future__ import annotations

import os
import subprocess
import sys
import textwrap
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption(
        "--lexxos-sim-lib",
        action="store",
        default=None,
        help="Path to liblexxos_sim.dylib/.so/.dll. Defaults to LEXXOS_SIM_LIB or build/full-dev-sim/src/liblexxos_sim.*",
    )


@pytest.fixture(scope="session")
def repo_root() -> Path:
    return REPO_ROOT


@pytest.fixture(scope="session")
def sim_lib(pytestconfig: pytest.Config, repo_root: Path) -> Path:
    configured = pytestconfig.getoption("--lexxos-sim-lib") or os.environ.get("LEXXOS_SIM_LIB")
    candidates: list[Path] = []
    if configured:
        library = Path(configured).expanduser()
        if not library.is_file():
            pytest.fail(f"Explicit simulator library does not exist: {library}")
        return library.resolve()
    candidates.extend(
        [
            repo_root / "build" / "full-dev-sim" / "src" / "liblexxos_sim.dylib",
            repo_root / "build" / "full-dev-sim" / "src" / "liblexxos_sim.so",
            repo_root / "build" / "full-dev-sim" / "src" / "lexxos_sim.dll",
        ]
    )

    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()

    pytest.skip(
        "lexxos_sim shared library was not found; build it first with "
        "`python emulator/lexxos_emu/gui.py --profile full --build-only` "
        "or pass --lexxos-sim-lib=/path/to/liblexxos_sim.dylib"
    )


@pytest.fixture()
def run_emulator_scenario(repo_root: Path, sim_lib: Path):
    def run(script: str, *, extra_env: dict[str, str] | None = None, timeout: float = 15.0) -> \
    subprocess.CompletedProcess[str]:
        env = os.environ.copy()
        env["LEXXOS_SIM_LIB"] = str(sim_lib)
        env["PYTHONPATH"] = str(repo_root / "emulator") + os.pathsep + env.get("PYTHONPATH", "")
        if extra_env:
            env.update(extra_env)

        return subprocess.run(
            [sys.executable, "-c", textwrap.dedent(script)],
            cwd=repo_root,
            env=env,
            text=True,
            capture_output=True,
            timeout=timeout,
            check=False,
        )

    return run


def assert_scenario_ok(result: subprocess.CompletedProcess[str]) -> str:
    combined = result.stdout + result.stderr
    assert result.returncode == 0, combined
    return combined
