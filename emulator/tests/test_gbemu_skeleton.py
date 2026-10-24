from __future__ import annotations

from emulator.tests.conftest import assert_scenario_ok
from emulator.tests.test_gb_info import _create_test_rom


def _create_vram_pattern_rom() -> bytes:
    rom = bytearray(_create_test_rom())
    program = [
        0x31, 0xFE, 0xFF,  # LD SP,0xFFFE
        0x3E, 0x91,        # LD A,0x91
        0xE0, 0x40,        # LDH (FF40),A ; LCDC enable, BG, unsigned tile data
        0x3E, 0xE4,        # LD A,0xE4
        0xE0, 0x47,        # LDH (FF47),A ; DMG palette
        0x21, 0x00, 0x80,  # LD HL,0x8000 ; tile 0
    ]
    for value in [0xAA, 0x00, 0x55, 0x00, 0xAA, 0x00, 0x55, 0x00, 0xAA, 0x00, 0x55, 0x00, 0xAA, 0x00, 0x55, 0x00]:
        program.extend([0x3E, value, 0x22])  # LD A,value; LD (HL+),A
    program.extend([0x18, 0xFE])  # JR -2
    rom[0x150:0x150 + len(program)] = bytes(program)
    return bytes(rom)


def _create_quick_stop_rom() -> bytes:
    rom = bytearray(_create_test_rom())
    rom[0x150] = 0xD3
    return bytes(rom)
def test_gbemu_runs_cpu_and_renders_to_sim_display(run_emulator_scenario, tmp_path):
    sd_root = tmp_path / "sd-root"
    sd_root.mkdir()
    (sd_root / "TEST.GB").write_bytes(_create_vram_pattern_rom())

    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import KeyCode, LexxOsSim, VirtualBoard

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

            before = sim.display_info()
            sim.set_key(KeyCode.A, True)
            sim.write_line("gbemu --frames 2 --steps-per-frame 96 /mnt/sd/TEST.GB")
            output += sim.run_until_idle()
            sim.set_key(KeyCode.A, False)
            after = sim.display_info()

            pixels = sim.display_rgb565()
            sim.write_line("exit")
            sim.run_until_idle()

        assert "gbemu: loaded LEXXGBTEST" in output, output
        assert "gbemu: display 320x320 rgb565" in output, output
        assert "gbemu: mapper ROM ONLY frames=2 steps/frame=96" in output, output
        assert "gbemu: rendered frames: 2" in output, output
        assert "gbemu: cpu steps=" in output, output
        assert "status=ok" in output, output
        assert "gbemu: final PC=" in output, output
        assert "gbemu: io LCDC=" in output, output
        assert "gbemu: selected-bank=" in output, output
        assert "vram-nonzero=" in output, output
        assert "a:1" in output, output
        assert after.frame_counter >= before.frame_counter + 2, (before, after, output)
        assert len(pixels) == 320 * 320 * 2
        assert len(set(pixels)) > 1
        print(output)
        ''',
        extra_env={"LEXXOS_TEST_SD_DIR": str(sd_root)},
        timeout=20.0,
    )
    assert_scenario_ok(result)


def test_gbemu_without_rom_opens_launcher_and_exits(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os

        from lexxos_emu import KeyCode, LexxOsSim, VirtualBoard

        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico()) as sim:
            sim.boot()
            output = sim.run_until_output_contains("lexx:/>", timeout=5.0)
            before = sim.display_info()
            sim.write_line("gbemu")
            sim.run_ms(200)
            after_open = sim.display_info()
            pixels = sim.display_rgb565()
            sim.set_key(KeyCode.BACKSPACE, True)
            sim.run_ms(30)
            sim.set_key(KeyCode.BACKSPACE, False)
            output += sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write_line("exit")
            sim.run_until_idle()

        assert after_open.frame_counter > before.frame_counter, (before, after_open, output)
        assert len(set(pixels)) > 1
        print(output)
        ''',
        timeout=20.0,
    )
    assert_scenario_ok(result)


def test_gbemu_launcher_runs_selected_rom(run_emulator_scenario, tmp_path):
    sd_root = tmp_path / "sd-root"
    rom_dir = sd_root / "gb"
    rom_dir.mkdir(parents=True)
    (rom_dir / "TEST.GB").write_bytes(_create_quick_stop_rom())

    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import KeyCode, LexxOsSim, VirtualBoard

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

            sim.write_line("gbemu")
            sim.run_ms(200)
            before_launch = sim.display_info().frame_counter
            sim.set_key(KeyCode.Z, True)
            sim.run_ms(30)
            sim.set_key(KeyCode.Z, False)
            # Selecting a ROM redraws once before execution, then again on return.
            import time
            deadline = time.monotonic() + 5.0
            while sim.display_info().frame_counter < before_launch + 2:
                assert time.monotonic() < deadline, "launcher did not redraw after ROM exit"
                sim.run_ms(20)
            output += sim.read_all()
            sim.set_key(KeyCode.BACKSPACE, True)
            sim.run_ms(30)
            sim.set_key(KeyCode.BACKSPACE, False)
            output += sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write_line("cat /mnt/sd/apps/gbemu/settings.ini")
            output += sim.run_until_output_contains("last_rom = /mnt/sd/gb/TEST.GB", timeout=5.0)
            output += sim.run_until_idle()
            sim.write_line("exit")
            sim.run_until_idle()

        assert "last_rom = /mnt/sd/gb/TEST.GB" in output, output
        assert "speed_mode = playable" in output, output
        print(output)
        ''',
        extra_env={"LEXXOS_TEST_SD_DIR": str(sd_root)},
        timeout=20.0,
    )
    assert_scenario_ok(result)
