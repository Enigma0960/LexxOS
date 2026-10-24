from __future__ import annotations

from emulator.tests.conftest import assert_scenario_ok


def test_gb_irq_services_interrupts_and_wakes_halt(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard

        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico()) as sim:
            sim.boot()
            output = sim.run_until_output_contains("lexx:/> ", timeout=5.0)
            sim.write_line("gb-irq")
            output += sim.run_until_output_contains("irq: ok", timeout=10.0)
            output += sim.run_until_idle(rounds=20)
            sim.write_line("exit")
            sim.run_until_idle()

        assert "gb-irq: CPU interrupt handling smoke test" in output, output
        assert "ie-timer[0xFFFF]: 0x04" in output, output
        assert "if-before-timer[0xFF0F]: 0xE4" in output, output
        assert "#0 pc=0x0100 op=0x00 cycles=20 status=ok irq=0x04 vector=0x0050 | PC=0x0050 SP=0xFFFC" in output, output
        assert "if-after-timer-service[0xFF0F]: 0xE0" in output, output
        assert "if-before-priority[0xFF0F]: 0xF5" in output, output
        assert "#1 pc=0x0100 op=0x00 cycles=20 status=ok irq=0x01 vector=0x0040 | PC=0x0040 SP=0xFFFC" in output, output
        assert "if-after-vblank-service[0xFF0F]: 0xF4" in output, output
        assert "#2 pc=0xC000 op=0x76 cycles=4 status=halted" in output, output
        assert "#3 pc=0xC001 op=0x00 cycles=4 status=ok | PC=0xC002" in output, output
        assert "if-after-halt-wakeup[0xFF0F]: 0xE1" in output, output
        assert "irq: ok" in output, output
        print(output)
        ''',
        timeout=20.0,
    )
    assert_scenario_ok(result)
