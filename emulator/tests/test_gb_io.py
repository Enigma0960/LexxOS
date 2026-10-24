from __future__ import annotations

from emulator.tests.conftest import assert_scenario_ok


def test_gb_io_exercises_joypad_timer_and_ppu_registers(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard

        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico()) as sim:
            sim.boot()
            output = sim.run_until_output_contains("lexx:/> ", timeout=5.0)
            sim.write_line("gb-io")
            output += sim.run_until_output_contains("io: ok", timeout=10.0)
            output += sim.run_until_idle(rounds=20)
            sim.write_line("exit")
            sim.run_until_idle()

        assert "gb-io: minimal joypad/timer/ppu IO model" in output, output
        assert "p1-default[0xFF00]: 0xFF" in output, output
        assert "p1-buttons-idle[0xFF00]: 0xDF" in output, output
        assert "p1-buttons-a-start[0xFF00]: 0xD6" in output, output
        assert "if-after-buttons[0xFF0F]: 0xF0" in output, output
        assert "p1-dpad-right-down[0xFF00]: 0xE6" in output, output
        assert "if-after-dpad[0xFF0F]: 0xF0" in output, output
        assert "div-initial[0xFF04]: 0x00" in output, output
        assert "div-after-256[0xFF04]: 0x01" in output, output
        assert "ly-after-456[0xFF44]: 0x01" in output, output
        assert "tima-after-16[0xFF05]: 0xFF" in output, output
        assert "tima-after-overflow[0xFF05]: 0xA7" in output, output
        assert "if-after-timer[0xFF0F]: 0xE4" in output, output
        assert "tac[0xFF07]: 0xFD" in output, output
        assert "io: ok" in output, output
        print(output)
        ''',
        timeout=20.0,
    )
    assert_scenario_ok(result)
