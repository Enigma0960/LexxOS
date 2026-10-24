from __future__ import annotations

from emulator.tests.conftest import assert_scenario_ok
from emulator.tests.test_gbemu_skeleton import _create_vram_pattern_rom


def test_gbemu_trace_and_cycle_budget(run_emulator_scenario, tmp_path):
    sd_root = tmp_path / "sd-root"
    sd_root.mkdir()
    (sd_root / "TEST.GB").write_bytes(_create_vram_pattern_rom())

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

            sim.write_line("gbemu --frames 2 --cycles-per-frame 2048 --trace /mnt/sd/TEST.GB")
            output += sim.run_until_idle()
            sim.write_line("exit")
            sim.run_until_idle()

        assert "gbemu: loaded LEXXGBTEST" in output, output
        assert "cycles/frame=2048" in output, output
        assert "frame 0: steps=" in output, output
        assert "frame 1: steps=" in output, output
        assert "gbemu: final PC=" in output, output
        assert "gbemu: io LCDC=" in output, output
        assert "vram-nonzero=" in output, output
        assert "vram-writes=" in output, output
        assert "status=ok" in output, output
        print(output)
        ''',
        extra_env={"LEXXOS_TEST_SD_DIR": str(sd_root)},
        timeout=20.0,
    )
    assert_scenario_ok(result)


def test_gbemu_until_vram_warmup(run_emulator_scenario, tmp_path):
    sd_root = tmp_path / "sd-root"
    sd_root.mkdir()
    (sd_root / "TEST.GB").write_bytes(_create_vram_pattern_rom())
    scenario = """
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
    sim.write_line("gbemu --frames 1 --until-vram --max-cycles 8192 /mnt/sd/TEST.GB")
    output += sim.run_until_idle()
    sim.write_line("exit")
    sim.run_until_idle()
assert "gbemu: warmup until-vram" in output, output
assert "vram-writes=" in output, output
assert "gbemu: rendered frames: 1" in output, output
assert "status=ok" in output, output
print(output)
"""
    result = run_emulator_scenario(
        scenario,
        extra_env={"LEXXOS_TEST_SD_DIR": str(sd_root)},
        timeout=20.0,
    )
    assert_scenario_ok(result)


def test_gbemu_until_visible_warmup(run_emulator_scenario, tmp_path):
    sd_root = tmp_path / "sd-root"
    sd_root.mkdir()
    (sd_root / "TEST.GB").write_bytes(_create_vram_pattern_rom())
    scenario = """
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
    sim.write_line("gbemu --frames 1 --until-visible --max-cycles 8192 /mnt/sd/TEST.GB")
    output += sim.run_until_idle()
    sim.write_line("exit")
    sim.run_until_idle()
assert "gbemu: warmup until-visible" in output, output
assert "vram-nonzero=" in output, output
assert "gbemu: rendered frames: 1" in output, output
assert "status=ok" in output, output
print(output)
"""
    result = run_emulator_scenario(
        scenario,
        extra_env={"LEXXOS_TEST_SD_DIR": str(sd_root)},
        timeout=20.0,
    )
    assert_scenario_ok(result)


def test_gbemu_until_bg_ready_warmup(run_emulator_scenario, tmp_path):
    sd_root = tmp_path / 'sd-root'
    sd_root.mkdir()
    (sd_root / 'TEST.GB').write_bytes(_create_vram_pattern_rom())
    scenario = r'''
import os
from lexxos_emu import LexxOsSim, VirtualBoard

board = VirtualBoard.dev_pico(sd_directory=os.environ['LEXXOS_TEST_SD_DIR'], sd_inserted=True)
with LexxOsSim(os.environ['LEXXOS_SIM_LIB'], board) as sim:
    sim.set_sd_inserted(True)
    sim.boot()
    output = sim.run_until_output_contains('lexx:/>', timeout=5.0)
    sim.run_ms(100)
    output += sim.read_all()
    if '/mnt/sd' not in output:
        sim.write_line('mount /dev/sd0 /mnt/sd fat')
        output += sim.run_until_idle()

    sim.write_line('gbemu --frames 1 --until-bg-ready --min-tile-data 1 --force-visible --max-cycles 8192 /mnt/sd/TEST.GB')
    output += sim.run_until_idle()
    sim.write_line('exit')
    sim.run_until_idle()

assert 'gbemu: warmup until-bg-ready' in output, output
assert 'tile-data=' in output, output
assert 'map=' in output, output
assert 'vram-ranges:' in output, output
assert 'force-visible=1' in output, output
assert 'gbemu: rendered frames: 1' in output, output
assert 'status=ok' in output, output
print(output)
'''
    result = run_emulator_scenario(
        scenario,
        extra_env={'LEXXOS_TEST_SD_DIR': str(sd_root)},
        timeout=20.0,
    )
    assert_scenario_ok(result)
