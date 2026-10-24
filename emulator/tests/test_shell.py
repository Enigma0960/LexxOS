from __future__ import annotations

from emulator.tests.conftest import assert_scenario_ok


def test_shell_help_smoke(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard

        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico()) as sim:
            sim.boot()
            output = sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write_line("help")
            output += sim.run_until_idle()
            sim.write_line("exit")
            output += sim.run_until_idle()

        assert "LexxOS shell" in output
        assert "board: LexxOS simulated board" in output
        assert "help - show program list" in output
        assert "ls - list VFS directory" in output
        assert "exit - exit host simulation" in output
        print(output)
        '''
    )
    assert_scenario_ok(result)


def test_root_vfs_layout(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard

        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico()) as sim:
            sim.boot()
            sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write_line("ls /")
            output = sim.run_until_idle()
            sim.write_line("exit")
            sim.run_until_idle()

        for expected in ("| dir  | dev", "| dir  | mnt", "| dir  | proc", "| dir  | sys", "| dir  | tmp"):
            assert expected in output, output
        print(output)
        '''
    )
    assert_scenario_ok(result)


def test_vfs_tmpfs_pseudo_files_and_path_normalization(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard

        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico()) as sim:
            sim.boot()
            output = sim.run_until_output_contains("lexx:/>", timeout=5.0)
            for command in (
                "cd /proc/../tmp",
                "pwd",
                "write vfs-note.txt alpha",
                "append vfs-note.txt beta",
                "mv vfs-note.txt vfs-renamed.txt",
                "cat vfs-renamed.txt",
                "stat vfs-renamed.txt",
                "cat /proc/uptime",
                "cat /sys/board",
                "mount",
                "rm vfs-renamed.txt",
                "tmp-info",
            ):
                sim.write_line(command)
                output += sim.run_until_idle()
            sim.write_line("exit")
            output += sim.run_until_idle()

        normalized = output.replace("\r\n", "\n")
        assert "\n/tmp\n" in normalized, output
        assert "\nalpha\nbeta\n" in normalized, output
        assert "path: /tmp/vfs-renamed.txt" in output
        assert "type: file" in output
        assert "size: 11 bytes" in output
        assert " ms" in output
        assert "id: dev-sim" in output
        assert "name: LexxOS simulated board" in output
        assert "| rootfs | /" in output
        assert "| virtualfs | ready" in output
        assert "files: 0/8 used, 8 free" in output
        print(output)
        '''
    )
    assert_scenario_ok(result)


def test_tasks_report_execution_metadata(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard

        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico()) as sim:
            sim.boot()
            output = sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write_line("tasks")
            output += sim.run_until_idle()
            sim.write_line("cat /proc/tasks")
            output += sim.run_until_idle()
            sim.write_line("exit")
            output += sim.run_until_idle()

        assert "started" in output, output
        assert "class" in output, output
        assert "core" in output, output
        assert "console" in output, output
        assert "system" in output, output
        assert "any" in output, output
        print(output)
        '''
    )
    assert_scenario_ok(result)


def test_shell_arrow_left_insert(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard
        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico()) as sim:
            sim.boot()
            output = sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write("echo ac\x1b[Db\n")
            output += sim.run_until_idle()
            sim.write_line("exit")
            output += sim.run_until_idle()
        assert "\nabc\n" in output.replace("\r\n", "\n"), output
        assert "unknown program" not in output, output
        print(output)
        '''
    )
    assert_scenario_ok(result)


def test_shell_backspace_inside_line(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard
        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico()) as sim:
            sim.boot()
            output = sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write("echo axbc\x1b[D\x1b[D\x7f\n")
            output += sim.run_until_idle()
            sim.write_line("exit")
            output += sim.run_until_idle()
        assert "\nabc\n" in output.replace("\r\n", "\n"), output
        assert "unknown program" not in output, output
        print(output)
        '''
    )
    assert_scenario_ok(result)


def test_shell_delete_inside_line(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard
        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico()) as sim:
            sim.boot()
            output = sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write("echo axbc\x1b[D\x1b[D\x1b[D\x1b[3~\n")
            output += sim.run_until_idle()
            sim.write_line("exit")
            output += sim.run_until_idle()
        assert "\nabc\n" in output.replace("\r\n", "\n"), output
        assert "unknown program" not in output, output
        print(output)
        '''
    )
    assert_scenario_ok(result)


def test_shell_history_up_reexecutes_last_command(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard
        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico()) as sim:
            sim.boot()
            output = sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write_line("echo history-one")
            output += sim.run_until_idle()
            sim.write("\x1b[A")
            output += sim.run_until_idle()
            sim.write("\n")
            output += sim.run_until_idle()
            sim.write_line("exit")
            output += sim.run_until_idle()
        normalized = output.replace("\r\n", "\n")
        assert normalized.count("\nhistory-one\n") == 2, normalized
        assert "unknown program" not in normalized, normalized
        print(output)
        '''
    )
    assert_scenario_ok(result)


def test_shell_history_down_restores_draft(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard
        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico()) as sim:
            sim.boot()
            sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write_line("echo history-base")
            sim.run_until_idle()
            sim.write("echo draft")
            sim.run_until_idle()
            sim.write("\x1b[A")
            sim.run_until_idle()
            sim.write("\x1b[B")
            sim.run_until_idle()
            sim.write("\n")
            output = sim.run_until_idle()
            sim.write_line("exit")
            output += sim.run_until_idle()
        normalized = output.replace("\r\n", "\n")
        assert "\ndraft\n" in normalized, normalized
        assert "\nhistory-base\n" not in normalized, normalized
        assert "unknown program" not in normalized, normalized
        print(output)
        '''
    )
    assert_scenario_ok(result)


def test_shell_history_keeps_last_ten_commands(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard
        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico()) as sim:
            sim.boot()
            sim.run_until_output_contains("lexx:/>", timeout=5.0)
            for index in range(12):
                sim.write_line(f"echo item-{index}")
                sim.run_until_idle()
            output = ""
            for _ in range(10):
                sim.write("\x1b[A")
                output += sim.run_until_idle()
            sim.write("\n")
            output += sim.run_until_idle()
            sim.write_line("exit")
            output += sim.run_until_idle()
        normalized = output.replace("\r\n", "\n")
        assert "\nitem-2\n" in normalized, normalized
        assert "\nitem-1\n" not in normalized, normalized
        assert "unknown program" not in normalized, normalized
        print(output)
        '''
    )
    assert_scenario_ok(result)


def test_shell_tab_completes_unique_program_name(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard
        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico()) as sim:
            sim.boot()
            output = sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write("ec\tcompleted\n")
            output += sim.run_until_idle()
            sim.write_line("exit")
            output += sim.run_until_idle()
        normalized = output.replace("\r\n", "\n")
        assert "\ncompleted\n" in normalized, normalized
        assert "unknown program" not in normalized, normalized
        print(output)
        '''
    )
    assert_scenario_ok(result)


def test_shell_tab_lists_ambiguous_program_names(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard
        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico()) as sim:
            sim.boot()
            output = sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write("i2c-\t")
            output += sim.run_until_idle()
            sim.write("s\t\n")
            output += sim.run_until_idle()
            sim.write_line("exit")
            output += sim.run_until_idle()
        normalized = output.replace("\r\n", "\n")
        assert "  i2c-buses\n" in normalized, normalized
        assert "  i2c-scan\n" in normalized, normalized
        assert "scan I2C bus" in normalized, normalized
        assert "unknown program" not in normalized, normalized
        print(output)
        '''
    )
    assert_scenario_ok(result)


def test_shell_tab_completes_absolute_vfs_directory(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard
        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico()) as sim:
            sim.boot()
            output = sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write("ls /pr\t\n")
            output += sim.run_until_idle()
            sim.write_line("exit")
            output += sim.run_until_idle()
        normalized = output.replace("\r\n", "\n")
        assert "| file | meminfo" in normalized, normalized
        assert "| file | tasks" in normalized, normalized
        assert "| file | uptime" in normalized, normalized
        assert "unknown program" not in normalized, normalized
        print(output)
        '''
    )
    assert_scenario_ok(result)


def test_shell_tab_lists_ambiguous_vfs_path_names(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard
        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico()) as sim:
            sim.boot()
            output = sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write("cat /proc/\t")
            output += sim.run_until_idle()
            sim.write("u\t\n")
            output += sim.run_until_idle()
            sim.write_line("exit")
            output += sim.run_until_idle()
        normalized = output.replace("\r\n", "\n")
        assert "  meminfo" in normalized, normalized
        assert "  tasks" in normalized, normalized
        assert "  uptime" in normalized, normalized
        assert "unknown program" not in normalized, normalized
        print(output)
        '''
    )
    assert_scenario_ok(result)


def test_shell_tab_completes_relative_vfs_path_from_cwd(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard
        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico()) as sim:
            sim.boot()
            output = sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write_line("cd /proc")
            output += sim.run_until_idle()
            sim.write("cat u\t\n")
            output += sim.run_until_idle()
            sim.write_line("exit")
            output += sim.run_until_idle()
        normalized = output.replace("\r\n", "\n")
        assert "unknown program" not in normalized, normalized
        assert "not found" not in normalized.lower(), normalized
        print(output)
        '''
    )
    assert_scenario_ok(result)
