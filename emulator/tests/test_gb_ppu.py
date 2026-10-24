from __future__ import annotations

from emulator.tests.conftest import assert_scenario_ok


def test_gb_ppu_test_renders_background_to_sim_display(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard

        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico()) as sim:
            sim.boot()
            output = sim.run_until_output_contains("lexx:/> ", timeout=5.0)
            before = sim.display_info()
            sim.write_line("gb-ppu-test")
            output += sim.run_until_output_contains("ppu: ok", timeout=10.0)
            output += sim.run_until_idle(rounds=20)
            after = sim.display_info()
            pixels = sim.display_rgb565()
            sim.write_line("exit")
            sim.run_until_idle()

        assert "gb-ppu-test: rendered synthetic DMG background" in output, output
        assert "display: 320x320 rgb565" in output, output
        assert "framebuffer: 160x144 indexed2" in output, output
        assert "scale: 2" in output, output
        assert "source: 160x144 @ 0,0" in output, output
        assert "destination: 320x288 @ 0,16" in output, output
        assert after.frame_counter >= before.frame_counter + 1, (before, after, output)
        assert len(pixels) == 320 * 320 * 2
        assert len(set(pixels)) > 1
        print(output)
        ''',
        timeout=20.0,
    )
    assert_scenario_ok(result)
