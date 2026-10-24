from __future__ import annotations

from emulator.tests.conftest import assert_scenario_ok
from emulator.tests.test_gb_info import _NINTENDO_LOGO


_BANK_SIZE = 16 * 1024


def _write_header(rom: bytearray, title: bytes, cartridge_type: int, rom_size_code: int, ram_size_code: int) -> None:
    rom[0x100:0x104] = b"\x00\xC3\x50\x01"
    rom[0x104:0x134] = _NINTENDO_LOGO
    rom[0x134:0x144] = title[:16].ljust(16, b"\0")
    rom[0x143] = 0x00
    rom[0x146] = 0x00
    rom[0x147] = cartridge_type
    rom[0x148] = rom_size_code
    rom[0x149] = ram_size_code
    rom[0x14A] = 0x01
    rom[0x14B] = 0x33
    rom[0x14C] = 0x00

    checksum = 0
    for value in rom[0x134:0x14D]:
        checksum = (checksum - value - 1) & 0xFF
    rom[0x14D] = checksum

    global_checksum = sum(rom[index] for index in range(len(rom)) if index not in (0x14E, 0x14F)) & 0xFFFF
    rom[0x14E] = (global_checksum >> 8) & 0xFF
    rom[0x14F] = global_checksum & 0xFF


def _create_mbc1_rom() -> bytes:
    rom = bytearray(128 * 1024)
    for bank in range(len(rom) // _BANK_SIZE):
        start = bank * _BANK_SIZE
        rom[start:start + _BANK_SIZE] = bytes([0x10 + bank]) * _BANK_SIZE
    _write_header(rom, b"LEXXMBC1", 0x03, 0x02, 0x02)
    return bytes(rom)


def _create_mbc5_rom() -> bytes:
    rom = bytearray(1024 * 1024)
    for bank in range(len(rom) // _BANK_SIZE):
        start = bank * _BANK_SIZE
        rom[start:start + _BANK_SIZE] = bytes([bank & 0xFF]) * _BANK_SIZE
    _write_header(rom, b"LEXXMBC5", 0x1B, 0x05, 0x03)
    return bytes(rom)


def test_gb_cart_reads_mbc1_banks(run_emulator_scenario, tmp_path):
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
            output = sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.run_ms(100)
            output += sim.read_all()
            if "/mnt/sd" not in output:
                sim.write_line("mount /dev/sd0 /mnt/sd fat")
                output += sim.run_until_idle()

            sim.write_line("gb-cart --bank 2 /mnt/sd/MBC1.GB")
            output += sim.run_until_output_contains("after-switch[4000]", timeout=10.0)
            output += sim.run_until_idle(rounds=20)
            sim.write_line("exit")
            sim.run_until_idle()

        assert "gb-cart: loaded LEXXMBC1" in output, output
        assert "mapper: MBC1 (MBC1+RAM+BATTERY, 0x03)" in output, output
        assert "mapper supported: yes" in output, output
        assert "features: ram:1 battery:1 timer:0 rumble:0" in output, output
        assert "rom banks: 8 (131072 bytes)" in output, output
        assert "ram banks: 1 (8192 bytes)" in output, output
        assert "selected bank: 1" in output, output
        assert "selected[4000]: 0x11" in output, output
        assert "after switch bank: 2" in output, output
        assert "after-switch[4000]: 0x12" in output, output
        print(output)
        ''',
        extra_env={"LEXXOS_TEST_SD_DIR": str(sd_root)},
        timeout=20.0,
    )
    assert_scenario_ok(result)


def test_gb_cart_reads_mbc5_high_bank(run_emulator_scenario, tmp_path):
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
            output = sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.run_ms(100)
            output += sim.read_all()
            if "/mnt/sd" not in output:
                sim.write_line("mount /dev/sd0 /mnt/sd fat")
                output += sim.run_until_idle()

            sim.write_line("gb-cart --bank 33 /mnt/sd/MBC5.GB")
            output += sim.run_until_output_contains("after-switch[4000]", timeout=10.0)
            output += sim.run_until_idle(rounds=20)
            sim.write_line("exit")
            sim.run_until_idle()

        assert "gb-cart: loaded LEXXMBC5" in output, output
        assert "mapper: MBC5 (MBC5+RAM+BATTERY, 0x1B)" in output, output
        assert "mapper supported: yes" in output, output
        assert "features: ram:1 battery:1 timer:0 rumble:0" in output, output
        assert "rom banks: 64 (1048576 bytes)" in output, output
        assert "ram banks: 4 (32768 bytes)" in output, output
        assert "selected bank: 1" in output, output
        assert "selected[4000]: 0x01" in output, output
        assert "after switch bank: 33" in output, output
        assert "after-switch[4000]: 0x21" in output, output
        print(output)
        ''',
        extra_env={"LEXXOS_TEST_SD_DIR": str(sd_root)},
        timeout=20.0,
    )
    assert_scenario_ok(result)
