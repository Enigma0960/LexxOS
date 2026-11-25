from __future__ import annotations

import pytest
from lexxos_emu.fat32_image import create_fat32_image_from_directory
from lexxos_emu.fat32_export import export_fat32_snapshot
from tools.emulator.tests.conftest import assert_scenario_ok


@pytest.mark.parametrize("capacity,cluster_bytes", [("256M", 2048), ("16G", 32768)])
def test_portable_benchmark_file_boundaries(run_emulator_scenario, tmp_path, capacity, cluster_bytes):
    source = tmp_path / "source"
    source.mkdir()
    original = bytes((i * 7 + i // 512) % 256 for i in range(32658))
    (source / "original.bin").write_bytes(original)
    (source / "keep").mkdir()
    (source / "keep" / "sentinel.txt").write_text("preserve me")
    image = create_fat32_image_from_directory(source, tmp_path / "sd.img", capacity=capacity)
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard
        board = VirtualBoard.dev_pico(sd_image=os.environ["LEXXOS_TEST_SD_IMAGE"], sd_inserted=True, sd_writable=True)
        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], board) as sim:
            sim.set_sd_inserted(True)
            sim.boot()
            sim.run_until_output_contains("lexx:/>", timeout=5)
            sim.run_ms(100)
            sim.read_all()
            def command(text):
                sim.write_line(text)
                return sim.run_until_output_contains("lexx:/>", timeout=15)
            # Пакет из 8 блоков и последний неполный пакет из одного блока.
            output = command("sd-bench --device /dev/sd0 --batch 8 --start 1 17 /mnt/sd/original.bin")
            assert "single/batch requests=17/3" in output, output
            assert "file verified bytes=32658" in output and "sd-bench: ok" in output, output
            output = command("sd-bench --device sd0 --write --bytes 32658 17 /mnt/sd/bench-new")
            assert "file verified bytes=32658" in output, output
            assert "cluster-bytes=" + os.environ["LEXXOS_TEST_CLUSTER_BYTES"] in output, output
            if os.environ["LEXXOS_TEST_CLUSTER_BYTES"] == "32768":
                assert "file verified bytes=65553" in output, output
            assert "sd-bench: ok" in output, output
            print(output)
            output = command("sd-bench --device sd0 --write 1 /mnt/sd/keep")
            assert "refusing to overwrite" in output, output
            output = command("sd-bench --device sd0 1 /tmp/not-on-sd")
            assert "file path must belong to the selected device" in output, output
            output = command("sd-bench --device sd1 1")
            assert "device unavailable" in output, output
            output = command("sd-bench --device sd0 --start 18446744073709551615 1")
            assert "invalid-range" in output, output
            command("sync")
            sim.write_line("exit")
            sim.run_until_idle()
        ''',
        extra_env={"LEXXOS_TEST_SD_IMAGE": str(image), "LEXXOS_TEST_CLUSTER_BYTES": str(cluster_bytes)},
        timeout=45,
    )
    assert_scenario_ok(result)
    exported = export_fat32_snapshot(image, tmp_path / "export")
    assert (exported / "original.bin").read_bytes() == original
    assert (exported / "keep" / "sentinel.txt").read_text() == "preserve me"
    assert not (exported / "bench-new").exists()
