from __future__ import annotations

from emulator.tests.conftest import assert_scenario_ok
from emulator.tests.test_gb_cart import _create_mbc1_rom, _create_mbc5_rom


def test_gb_bus_maps_mbc1_and_internal_memory(run_emulator_scenario, tmp_path):
    sd_root = tmp_path / "sd-root"
    sd_root.mkdir()
    (sd_root / "MBC1.GB").write_bytes(_create_mbc1_rom())

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

            sim.write_line("gb-bus --bank 2 /mnt/sd/MBC1.GB")
            output += sim.run_until_output_contains("bus: ok", timeout=10.0)
            output += sim.run_until_idle(rounds=20)
            sim.write_line("exit")
            sim.run_until_idle()

        assert "gb-bus: loaded LEXXMBC1" in output, output
        assert "mapper: MBC1 (MBC1+RAM+BATTERY, 0x03)" in output, output
        assert "memory: vram=8192 wram=8192 oam=160 io=128 hram=127" in output, output
        assert "selected bank before: 1" in output, output
        assert "switchable-before[0x4000]: 0x11" in output, output
        assert "selected bank after: 2" in output, output
        assert "switchable-after[0x4000]: 0x12" in output, output
        assert "vram[0x8000]: 0x12" in output, output
        assert "wram[0xC000]: 0x34" in output, output
        assert "echo[0xE000]: 0x34" in output, output
        assert "wram-after-echo-write[0xC001]: 0x56" in output, output
        assert "oam[0xFE00]: 0x78" in output, output
        assert "unusable[0xFEA0]: 0xFF" in output, output
        assert "io[0xFF01]: 0xAB" in output, output
        assert "joypad-buttons[0xFF00]: 0xDF" in output, output
        assert "hram[0xFF80]: 0xCD" in output, output
        assert "ie[0xFFFF]: 0x1F" in output, output
        assert "bus: ok" in output, output
        print(output)
        ''',
        extra_env={"LEXXOS_TEST_SD_DIR": str(sd_root)},
        timeout=20.0,
    )
    assert_scenario_ok(result)


def test_gb_bus_switches_mbc5_high_bank(run_emulator_scenario, tmp_path):
    sd_root = tmp_path / "sd-root"
    sd_root.mkdir()
    (sd_root / "MBC5.GB").write_bytes(_create_mbc5_rom())

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

            sim.write_line("gb-bus --bank 33 /mnt/sd/MBC5.GB")
            output += sim.run_until_output_contains("bus: ok", timeout=10.0)
            output += sim.run_until_idle(rounds=20)
            sim.write_line("exit")
            sim.run_until_idle()

        assert "gb-bus: loaded LEXXMBC5" in output, output
        assert "mapper: MBC5 (MBC5+RAM+BATTERY, 0x1B)" in output, output
        assert "selected bank before: 1" in output, output
        assert "switchable-before[0x4000]: 0x01" in output, output
        assert "selected bank after: 33" in output, output
        assert "switchable-after[0x4000]: 0x21" in output, output
        assert "bus: ok" in output, output
        print(output)
        ''',
        extra_env={"LEXXOS_TEST_SD_DIR": str(sd_root)},
        timeout=20.0,
    )
    assert_scenario_ok(result)
