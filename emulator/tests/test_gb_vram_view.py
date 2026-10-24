from __future__ import annotations

from emulator.tests.conftest import assert_scenario_ok
from emulator.tests.test_gbemu_skeleton import _create_vram_pattern_rom


def test_gb_vram_view_renders_rom_tile_data(run_emulator_scenario, tmp_path):
    sd_root = tmp_path / "sd-root"
    sd_root.mkdir()
    (sd_root / "TEST.GB").write_bytes(_create_vram_pattern_rom())

    scenario = r"""
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

    before = sim.display_info()
    sim.write_line("gb-vram-view --trace /mnt/sd/TEST.GB")
    output += sim.run_until_idle()
    after = sim.display_info()
    pixels = sim.display_rgb565()
    sim.write_line("exit")
    sim.run_until_idle()

assert "gb-vram-view: warmup steps=" in output, output
assert "gb-vram-view: rendered tile data" in output, output
assert "tile-data: base=0x8000" in output, output
assert "vram-view: ok" in output, output
assert after.frame_counter >= before.frame_counter + 1, (before, after, output)
assert len(pixels) == 320 * 320 * 2
assert len(set(pixels)) > 1
print(output)
"""

    result = run_emulator_scenario(
        scenario,
        extra_env={"LEXXOS_TEST_SD_DIR": str(sd_root)},
        timeout=20.0,
    )
    assert_scenario_ok(result)


def test_gb_vram_view_bg_and_auto_tile_modes(run_emulator_scenario, tmp_path):
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

    before = sim.display_info()
    sim.write_line('gb-vram-view --mode bg --target tile-data --min-target 1 --force-visible --bg-map 9800 --bg-tiles 8000 /mnt/sd/TEST.GB')
    output += sim.run_until_idle()
    after_bg = sim.display_info()
    sim.write_line('gb-vram-view --mode bg-scan --target tile-data --min-target 1 /mnt/sd/TEST.GB')
    output += sim.run_until_idle()
    after_scan = sim.display_info()
    sim.write_line('gb-vram-view --mode tiles --target tile-data --auto-tile /mnt/sd/TEST.GB')
    output += sim.run_until_idle()
    after_tiles = sim.display_info()
    sim.write_line('exit')
    sim.run_until_idle()

assert 'view-mode: bg' in output, output
assert 'background:' in output, output
assert 'force-visible=1' in output, output
assert 'bg-map=9800' in output, output
assert 'bg-tiles=8000' in output, output
assert 'view-mode: bg-scan' in output, output
assert 'background-scan:' in output, output
assert 'tile-data-first-nonzero:' in output, output
assert 'vram-view: ok' in output, output
assert after_bg.frame_counter >= before.frame_counter + 1, (before, after_bg, output)
assert after_tiles.frame_counter >= after_bg.frame_counter + 1, (after_bg, after_tiles, output)
assert after_scan.frame_counter >= after_bg.frame_counter + 1, (after_bg, after_scan, output)
assert after_tiles.frame_counter >= after_scan.frame_counter + 1, (after_scan, after_tiles, output)
print(output)
'''

    result = run_emulator_scenario(
        scenario,
        extra_env={'LEXXOS_TEST_SD_DIR': str(sd_root)},
        timeout=20.0,
    )
    assert_scenario_ok(result)
