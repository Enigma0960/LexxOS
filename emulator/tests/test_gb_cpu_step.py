from __future__ import annotations

from emulator.tests.conftest import assert_scenario_ok
from emulator.tests.test_gb_cart import _write_header


def _create_cpu_step_rom() -> bytes:
    rom = bytearray(32 * 1024)
    rom[0x100:0x104] = b"\x00\xC3\x50\x01"
    rom[0x150:0x164] = bytes(
        [
            0x31, 0xFE, 0xFF,  # LD SP,0xFFFE
            0x3E, 0x12,  # LD A,0x12
            0x06, 0x34,  # LD B,0x34
            0x80,  # ADD A,B -> A=0x46
            0xE0, 0x0F,  # LDH (0x0F),A
            0xF0, 0x0F,  # LDH A,(0x0F)
            0xEA, 0x00, 0xC0,  # LD (0xC000),A
            0xFA, 0x00, 0xC0,  # LD A,(0xC000)
            0xCB, 0x7F,  # BIT 7,A
            0x18, 0xFE,  # JR -2
        ]
    )
    _write_header(rom, b"LEXXCPU", 0x00, 0x00, 0x00)
    return bytes(rom)


def test_gb_cpu_step_executes_basic_instructions(run_emulator_scenario, tmp_path):
    sd_root = tmp_path / "sd-root"
    sd_root.mkdir()
    (sd_root / "CPU.GB").write_bytes(_create_cpu_step_rom())

    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard

        board = VirtualBoard.dev_pico(sd_directory=os.environ["LEXXOS_TEST_SD_DIR"], sd_inserted=True)
        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], board) as sim:
            sim.set_sd_inserted(True)
            sim.boot()
            output = sim.run_until_output_contains("lexx:/> ", timeout=5.0)
            sim.run_ms(100)
            output += sim.read_all()
            if "/mnt/sd" not in output:
                sim.write_line("mount /dev/sd0 /mnt/sd fat")
                output += sim.run_until_idle()

            sim.write_line("gb-cpu-step --steps 11 /mnt/sd/CPU.GB")
            output += sim.run_until_output_contains("cpu: ok steps=11", timeout=10.0)
            output += sim.run_until_idle(rounds=20)
            sim.write_line("exit")
            sim.run_until_idle()

        assert "gb-cpu-step: loaded LEXXCPU" in output, output
        assert "mapper: ROM ONLY (ROM ONLY, 0x00)" in output, output
        assert "start: PC=0x0100 SP=0xFFFE AF=0x01B0" in output, output
        assert "#0 pc=0x0100 op=0x00 cycles=4 status=ok" in output, output
        assert "#1 pc=0x0101 op=0xC3 cycles=16 status=ok | PC=0x0150" in output, output
        assert "#2 pc=0x0150 op=0x31 cycles=12 status=ok | PC=0x0153 SP=0xFFFE" in output, output
        assert "#3 pc=0x0153 op=0x3E cycles=8 status=ok | PC=0x0155 SP=0xFFFE AF=0x12B0" in output, output
        assert "#4 pc=0x0155 op=0x06 cycles=8 status=ok | PC=0x0157 SP=0xFFFE AF=0x12B0 BC=0x3413" in output, output
        assert "#5 pc=0x0157 op=0x80 cycles=4 status=ok | PC=0x0158 SP=0xFFFE AF=0x4600" in output, output
        assert "#10 pc=0x0162 op=0xCB cb=0x7F cycles=8 status=ok" in output, output
        assert "cpu: ok steps=11" in output, output
        print(output)
        ''',
        extra_env={"LEXXOS_TEST_SD_DIR": str(sd_root)},
        timeout=20.0,
    )
    assert_scenario_ok(result)
