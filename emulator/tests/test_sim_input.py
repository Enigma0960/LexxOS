from __future__ import annotations

from emulator.tests.conftest import assert_scenario_ok


def test_sim_input_device_and_gamepad_adapter(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import KeyCode, LexxOsSim, VirtualBoard

        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico()) as sim:
            sim.boot()
            sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write_line("devices")
            output = sim.run_until_output_contains("lexx:/>", timeout=5.0)

            assert "sim-keyboard" in output and "| input" in output, output
            assert "sim-gamepad" in output and "| gamepad | ready" in output, output

            sim.set_key(KeyCode.Z, True)
            sim.write_line("input-test")
            output = sim.run_until_idle()
            assert "key-down: z" in output, output

            sim.write_line("keyboard-live -s 1 -i 20")
            output = sim.run_until_output_contains("keyboard-live: events", timeout=2.5)
            assert "keyboard-live: device=sim-keyboard" in output, output
            assert "pressed: z" in output, output

            sim.write_line("gamepad-test")
            output = sim.run_until_idle()
            assert "x:1" in output, output
            assert "a:0" in output, output
            assert "b:0" in output, output

            sim.set_key("z", False)
            sim.write_line("input-test")
            output = sim.run_until_idle()
            assert "key-up: z" in output, output

            sim.write_line("exit")
            sim.run_until_idle()
        '''
    )
    assert_scenario_ok(result)


def test_keyboard_live_can_be_cancelled_with_ctrl_c(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard

        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico()) as sim:
            sim.boot()
            output = sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write_line("keyboard-live -s 5 -i 20")
            output += sim.run_until_output_contains("keyboard-live: device=sim-keyboard", timeout=2.5)
            sim.write(b"\x03")
            output += sim.run_until_output_contains("keyboard-live: canceled", timeout=2.5)
            output += sim.run_until_idle()
            sim.write_line("exit")
            output += sim.run_until_idle()

        assert "keyboard-live: canceled" in output, output
        assert output.count("lexx:/>") >= 2, output
        print(output)
        '''
    )
    assert_scenario_ok(result)


def test_sim_input_pushes_characters_and_modifiers(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import InputModifier, KeyCode, LexxOsSim, VirtualBoard

        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico()) as sim:
            sim.boot()
            output = sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write_line("keyboard-live -s 1 -i 20")
            output += sim.run_until_output_contains("press and release keys now", timeout=2.5)

            sim.tap_key_event(KeyCode.TAB, modifiers=InputModifier.ALT)
            sim.tap_key_event(KeyCode.C, character=3, modifiers=InputModifier.CTRL)

            output += sim.run_until_output_contains("keyboard-live: events", timeout=2.5)
            sim.write_line("exit")
            output += sim.run_until_idle()

        assert "event: key-down tab mods=alt(0x01)" in output, output
        assert "event: key-up tab mods=alt(0x01)" in output, output
        assert "event: key-down c char='?' mods=ctrl(0x10)" in output, output
        assert "event: key-up c mods=ctrl(0x10)" in output, output
        print(output)
        '''
    )
    assert_scenario_ok(result)
