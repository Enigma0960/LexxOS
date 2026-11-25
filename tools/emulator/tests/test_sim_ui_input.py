"""Regression: simulated hardware keys reach the foreground .lexx UI queue.

Requires build/apps/ui-demo.lexx and a built simulator library. Every scenario
uses a separate process because the native simulator owns singleton state.
"""
from __future__ import annotations

from pathlib import Path

import pytest

from tools.emulator.tests.conftest import assert_scenario_ok


def test_display_tab_reaches_ui_demo(run_emulator_scenario, repo_root: Path):
    executable = repo_root / "build" / "apps" / "ui-demo.lexx"
    if not executable.is_file():
        pytest.skip("Build app-ui-demo first: build/apps/ui-demo.lexx is missing")

    result = run_emulator_scenario(
        r'''
        import os
        import tempfile
        import time
        from pathlib import Path

        from lexxos_emu import KeyCode, LexxOsSim, VirtualBoard

        with tempfile.TemporaryDirectory(prefix="lexxos-ui-input-") as directory:
            root = Path(directory)
            (root / "ui-demo.lexx").write_bytes(Path(os.environ["LEXXOS_UI_DEMO"]).read_bytes())
            board = VirtualBoard.dev_pico(
                sd_directory=root, sd_inserted=True,
                sd_capacity="256M", sd_filesystem="fat32", sd_writable=True,
            )
            with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], board) as sim:
                sim.boot()
                sim.run_until_output_contains("lexx:/>", timeout=5.0)
                start_frame = sim.display_info().frame_counter
                sim.write_line("/mnt/sd/ui-demo.lexx")
                deadline = time.monotonic() + 6.0
                while time.monotonic() < deadline:
                    sim.run_ms(10)
                    if sim.display_info().frame_counter >= start_frame + 2:
                        break
                else:
                    raise AssertionError("ui-demo did not render the system window")

                # The demo is event-driven; after the list is populated its
                # framebuffer must stay still until an input event is handled.
                for _ in range(10):
                    sim.run_ms(10)
                before = sim.display_rgb565()
                before_frame = sim.display_info().frame_counter

                sim.tap_key_event(KeyCode.TAB, character=9)
                deadline = time.monotonic() + 3.0
                while time.monotonic() < deadline:
                    sim.run_ms(10)
                    if sim.display_info().frame_counter > before_frame:
                        if sim.display_rgb565() != before:
                            break
                else:
                    raise AssertionError("Tab did not change LexxUI focus / framebuffer")

        ''',
        extra_env={"LEXXOS_UI_DEMO": str(executable)},
        timeout=30.0,
    )
    assert_scenario_ok(result)
