from __future__ import annotations

from emulator.tests.conftest import assert_scenario_ok
from emulator.tests.support.fat_image import create_fat16_image


def test_sd_no_card_reports_not_ready(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard

        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico()) as sim:
            sim.boot()
            sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write_line("storage")
            output = sim.run_until_idle()
            sim.write_line("block-devs")
            output += sim.run_until_idle()
            sim.write_line("mount /dev/sd0 /mnt/sd fat")
            output += sim.run_until_idle()
            sim.write_line("exit")
            sim.run_until_idle()

        assert "card detect: empty" in output
        assert "sd state: no-card" in output
        assert "block ready: no" in output
        assert "| sd0  | not-ready" in output
        assert "block device is not ready" in output
        print(output)
        '''
    )
    assert_scenario_ok(result)


def test_sd_fat_image_mounts_and_reads_file(run_emulator_scenario, tmp_path):
    image = create_fat16_image(tmp_path / "sd-fat16.img")
    expected_text = "Hello from LexxOS emulator tests"

    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard

        image_path = os.environ["LEXXOS_TEST_SD_IMAGE"]
        board = VirtualBoard.dev_pico(sd_image=image_path, sd_inserted=True)

        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], board) as sim:
            sim.set_sd_inserted(True)
            sim.boot()
            output = sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.run_ms(100)
            output += sim.read_all()

            sim.write_line("storage")
            output += sim.run_until_idle()
            sim.write_line("mount")
            output += sim.run_until_idle()

            if "/mnt/sd" not in output:
                sim.write_line("mount /dev/sd0 /mnt/sd fat")
                output += sim.run_until_idle()

            sim.write_line("ls /mnt/sd")
            output += sim.run_until_idle()
            sim.write_line("cat /mnt/sd/README.TXT")
            output += sim.run_until_idle()
            sim.write_line("exit")
            sim.run_until_idle()

        assert "card detect: inserted" in output
        assert "sd state: ready" in output
        assert "block ready: yes" in output
        assert "/mnt/sd" in output
        assert "readme.txt" in output.lower()
        assert "Hello from LexxOS emulator tests" in output, output
        print(output)
        ''',
        extra_env={"LEXXOS_TEST_SD_IMAGE": str(image)},
        timeout=20.0,
    )
    output = assert_scenario_ok(result)
    assert expected_text in output


def test_sd_host_directory_mounts_and_reads_file(run_emulator_scenario, tmp_path):
    sd_root = tmp_path / "sd-root"
    sd_root.mkdir()
    (sd_root / "README.TXT").write_text("Hello from host directory SD\n", encoding="utf-8")
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard
        sd_root = os.environ["LEXXOS_TEST_SD_DIR"]
        board = VirtualBoard.dev_pico(sd_directory=sd_root, sd_inserted=True)
        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], board) as sim:
            sim.set_sd_inserted(True)
            sim.boot()
            output = sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.run_ms(100)
            output += sim.read_all()
            if "/mnt/sd" not in output:
                sim.write_line("mount /dev/sd0 /mnt/sd fat")
                output += sim.run_until_idle()
            sim.write_line("ls /mnt/sd")
            output += sim.run_until_idle()
            sim.write_line("cat /mnt/sd/README.TXT")
            output += sim.run_until_idle()
            sim.write_line("exit")
            sim.run_until_idle()
        assert "/mnt/sd" in output
        assert "readme.txt" in output.lower()
        assert "Hello from host directory SD" in output
        print(output)
        ''',
        extra_env={"LEXXOS_TEST_SD_DIR": str(sd_root)},
        timeout=20.0,
    )
    assert_scenario_ok(result)
