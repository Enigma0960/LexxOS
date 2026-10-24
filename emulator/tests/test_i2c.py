from __future__ import annotations

from emulator.tests.conftest import assert_scenario_ok


def test_i2c_scan_can_see_optional_oled_device(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard

        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico(oled=True)) as sim:
            sim.boot()
            sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write_line("i2c-buses")
            output = sim.run_until_idle()
            sim.write_line("i2c-scan --quiet")
            output += sim.run_until_idle()
            sim.write_line("exit")
            sim.run_until_idle()

        assert "| debug | ready | free" in output
        assert "0x3C" in output
        print(output)
        '''
    )
    assert_scenario_ok(result)
