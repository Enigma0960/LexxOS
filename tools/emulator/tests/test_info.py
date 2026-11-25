"""Smoke tests for the generic VFS information command."""

from __future__ import annotations

from tools.emulator.tests.conftest import assert_scenario_ok


def test_info_files_and_directories(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard

        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico()) as sim:
            sim.boot()
            sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write_line("info /proc")
            proc = sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write_line("info /proc/uptime")
            uptime = sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write_line("write /tmp/note.txt hello")
            sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write_line("info /tmp/note.txt")
            note = sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write_line("info /tmp")
            tmp = sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write_line("help lexx-info")
            old = sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write_line("exit")
            sim.run_until_idle()

        assert "name: proc" in proc, proc
        assert "type: directory" in proc, proc
        assert "files:" in proc and "directories:" in proc, proc
        assert "name: uptime" in uptime and "type: " in uptime, uptime
        assert "created: " in uptime and "permissions: " in uptime, uptime
        assert "name: note.txt" in note and "type: text" in note and "size: " in note, note
        assert "name: tmp" in tmp and "files: 1" in tmp, tmp
        assert "lexx-info" in old, old
        print(proc + uptime + note + tmp + old)
        '''
    )
    assert_scenario_ok(result)


def test_info_shortcut_target_and_invalid_manifest(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard

        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico()) as sim:
            sim.boot()
            sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write_line("link create /tmp/snake.lnk /tmp/snake.lexx --fg")
            created = sim.run_until_idle()
            sim.write_line("info /tmp/snake.lnk")
            shortcut = sim.run_until_idle()
            sim.write_line("write /tmp/broken.lnk broken")
            sim.run_until_idle()
            sim.write_line("info /tmp/broken.lnk")
            broken = sim.run_until_idle()
            sim.write_line("exit")
            sim.run_until_idle()

        assert "link:" not in created, created
        assert "name: snake.lnk" in shortcut, shortcut
        assert "path: /tmp/snake.lnk" in shortcut, shortcut
        assert "type: shortcut" in shortcut, shortcut
        assert "target: /tmp/snake.lexx" in shortcut, shortcut
        assert "size: " in shortcut and "permissions: " in shortcut, shortcut
        assert "shortcut (invalid)" not in shortcut, shortcut
        assert "path: /tmp/broken.lnk" in broken, broken
        assert "type: shortcut (invalid)" in broken, broken
        assert "shortcut status: invalid-link" in broken, broken
        print(shortcut + broken)
        '''
    )
    assert_scenario_ok(result)
