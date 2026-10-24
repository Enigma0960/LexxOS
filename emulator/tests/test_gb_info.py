from __future__ import annotations

from emulator.tests.conftest import assert_scenario_ok

_NINTENDO_LOGO = bytes(
    [
        0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
        0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
        0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
        0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
        0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
        0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E,
    ]
)


def _create_test_rom() -> bytes:
    rom = bytearray(32 * 1024)
    rom[0x100:0x104] = b"\x00\xC3\x50\x01"
    rom[0x104:0x134] = _NINTENDO_LOGO
    rom[0x134:0x144] = b"LEXXGBTEST\0\0\0\0\0\0"
    rom[0x143] = 0x00
    rom[0x146] = 0x00
    rom[0x147] = 0x00
    rom[0x148] = 0x00
    rom[0x149] = 0x00
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
    return bytes(rom)


def test_gb_info_reads_rom_header_from_sd(run_emulator_scenario, tmp_path):
    sd_root = tmp_path / "sd-root"
    sd_root.mkdir()
    (sd_root / "TEST.GB").write_bytes(_create_test_rom())

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

            sim.write_line("gb-info /mnt/sd/TEST.GB")
            output += sim.run_until_idle()
            sim.write_line("exit")
            sim.run_until_idle()

        assert "title: LEXXGBTEST" in output, output
        assert "cartridge type: ROM ONLY (0x00)" in output, output
        assert "rom size: 32 KiB (0x00, 32768 bytes)" in output, output
        assert "ram size: none (0x00, 0 bytes)" in output, output
        assert "destination: Overseas (0x01)" in output, output
        assert "nintendo logo: ok" in output, output
        assert "header checksum: ok" in output, output
        assert "rom size check: ok" in output, output
        print(output)
        ''',
        extra_env={"LEXXOS_TEST_SD_DIR": str(sd_root)},
        timeout=20.0,
    )
    assert_scenario_ok(result)
