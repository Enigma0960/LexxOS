from __future__ import annotations

import subprocess

import pytest

from emulator.lexxos_emu import sim_build


@pytest.mark.parametrize("profile", ["minimal", "full"])
def test_simulator_build_always_reconfigures_and_builds_selected_profile(tmp_path, monkeypatch, profile):
    library = sim_build.simulator_library(tmp_path, profile)
    library.parent.mkdir(parents=True)
    library.write_bytes(b"existing")
    calls = []

    def run(command, **kwargs):
        calls.append((command, kwargs))

    monkeypatch.setattr(sim_build.subprocess, "run", run)
    monkeypatch.setenv("LEXXOS_CMAKE_BIN", "/path with spaces/cmake")
    assert sim_build.build_simulator(tmp_path, profile) == library.resolve()
    assert calls == [
        (["/path with spaces/cmake", "--preset", f"{profile}-dev-sim"], {"cwd": tmp_path, "check": True}),
        (["/path with spaces/cmake", "--build", "--preset", f"{profile}-dev-sim", "--parallel", "2"],
         {"cwd": tmp_path, "check": True}),
    ]


def test_failed_build_does_not_fall_back_to_stale_library(tmp_path, monkeypatch):
    library = sim_build.simulator_library(tmp_path, "minimal")
    library.parent.mkdir(parents=True)
    library.write_bytes(b"stale")

    def fail(command, **kwargs):
        raise subprocess.CalledProcessError(1, command)

    monkeypatch.setattr(sim_build.subprocess, "run", fail)
    with pytest.raises(subprocess.CalledProcessError):
        sim_build.build_simulator(tmp_path, "minimal")


def test_successful_command_without_artifact_is_an_error(tmp_path, monkeypatch):
    monkeypatch.setattr(sim_build.subprocess, "run", lambda *args, **kwargs: None)
    with pytest.raises(FileNotFoundError):
        sim_build.build_simulator(tmp_path, "full")
