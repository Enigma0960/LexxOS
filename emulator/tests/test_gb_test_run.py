from __future__ import annotations

from emulator.tests.conftest import assert_scenario_ok
from emulator.tests.test_gb_info import _create_test_rom


def _create_serial_pass_rom() -> bytes:
    rom = bytearray(_create_test_rom())
    program: list[int] = []
    for value in b"Passed\n":
        program.extend([
            0x3E, value,  # LD A,value
            0xE0, 0x01,   # LDH (FF01),A
            0x3E, 0x81,   # LD A,0x81
            0xE0, 0x02,   # LDH (FF02),A
            0xF0, 0x02,   # LDH A,(FF02)
            0xE6, 0x80,   # AND transfer-start flag
            0x20, 0xFA,   # JR NZ,-6 ; wait before sending the next byte
        ])
    program.extend([0x18, 0xFE])  # JR -2
    rom[0x150:0x150 + len(program)] = bytes(program)
    return bytes(rom)


def test_gb_test_run_captures_serial_output(run_emulator_scenario, tmp_path):
    sd_root = tmp_path / "sd-root"
    sd_root.mkdir()
    (sd_root / "SERIAL.GB").write_bytes(_create_serial_pass_rom())

    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard

        board = VirtualBoard.dev_pico(sd_directory=os.environ["LEXXOS_TEST_SD_DIR"], sd_inserted=True)
        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], board) as sim:
            sim.set_sd_inserted(True)
            sim.boot()
            output = sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.run_ms(100)
            output += sim.read_all()
            if "/mnt/sd" not in output:
                sim.write_line("mount /dev/sd0 /mnt/sd fat")
                output += sim.run_until_idle()

            sim.write_line("gb-test-run --cycles 32768 --trace /mnt/sd/SERIAL.GB")
            output += sim.run_until_idle()
            sim.write_line("exit")
            sim.run_until_idle()

        assert "gb-test-run: loaded LEXXGBTEST" in output, output
        assert "serial-size=" in output, output
        assert "Passed" in output, output
        assert "gb-test-run: result=passed" in output, output
        print(output)
        ''',
        extra_env={"LEXXOS_TEST_SD_DIR": str(sd_root)},
        timeout=20.0,
    )
    assert_scenario_ok(result)
