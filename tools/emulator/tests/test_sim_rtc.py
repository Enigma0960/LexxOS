from __future__ import annotations

from tools.emulator.tests.conftest import assert_scenario_ok


def test_simulator_rtc_uses_host_utc_clock(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        import re
        import time
        from lexxos_emu import LexxOsSim, VirtualBoard

        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico()) as sim:
            sim.boot()
            sim.run_until_output_contains("lexx:/>", timeout=5.0)

            sim.write_line("rtc-status --source software")
            status = sim.run_until_output_contains("last-set: host", timeout=3.0)
            status += sim.run_until_idle()
            assert "source: software" in status, status
            assert "valid: yes" in status, status
            assert "last-set: host" in status, status
            match = re.search(r"unix:\s*(\d+)", status)
            assert match is not None, status
            assert abs(int(match.group(1)) - int(time.time())) <= 10, status

            sim.write_line("rtc-get --source software")
            output = sim.run_until_output_contains("rtc:", timeout=3.0)
            output += sim.run_until_idle()
            assert "rtc: " in output, output

            sim.write_line("exit")
            sim.run_until_idle()
        '''
    )
    assert_scenario_ok(result)
