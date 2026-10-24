from __future__ import annotations

from emulator.tests.conftest import assert_scenario_ok


def test_sim_display_device_and_test_pattern(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard

        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico()) as sim:
            sim.boot()
            sim.run_until_output_contains("lexx:/>", timeout=5.0)

            sim.write_line("devices")
            output = sim.run_until_output_contains("emu_display", timeout=2.0)
            output += sim.run_until_idle()
            assert "emu_display" in output, output
            assert "| display | ready" in output, output

            sim.write_line("display-info")
            output = sim.run_until_idle()
            assert "name: emu_display" in output, output
            assert "size: 320x320" in output, output
            assert "format: rgb565" in output, output

            before = sim.display_info()
            assert before.width == 320, before
            assert before.height == 320, before

            sim.write_line("display-test")
            output = sim.run_until_idle()
            assert "drawn: 320x320 native=rgb565" in output, output

            after = sim.display_info()
            assert after.frame_counter > before.frame_counter, (before, after)

            pixels = sim.display_rgb565()
            assert len(pixels) == 320 * 320 * 2
            assert len(set(pixels)) > 1

            sim.write_line("exit")
            sim.run_until_idle()
        '''
    )
    assert_scenario_ok(result)


def test_sim_display_uses_configured_size(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard

        board = VirtualBoard.dev_pico(display_size=(200, 120))
        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], board) as sim:
            sim.boot()
            sim.run_until_output_contains("lexx:/>", timeout=5.0)

            sim.write_line("display-info")
            output = sim.run_until_idle()
            assert "name: emu_display" in output, output
            assert "size: 200x120" in output, output

            before = sim.display_info()
            assert before.width == 200, before
            assert before.height == 120, before

            sim.write_line("display-test")
            output = sim.run_until_idle()
            assert "drawn: 200x120 native=rgb565" in output, output

            pixels = sim.display_rgb565()
            assert len(pixels) == 200 * 120 * 2

            sim.write_line("exit")
            sim.run_until_idle()
        '''
    )
    assert_scenario_ok(result)
