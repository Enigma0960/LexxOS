"""Exercise real host stdin through both a pipe and a redirected regular file."""
import json
import re
import runpy
import subprocess
import sys
import tempfile
from pathlib import Path


expected = {entry["name"] for entry in json.loads(Path(sys.argv[2]).read_text()) if entry["enabled"]} | {"shell"}

def check(result):
    assert result.returncode == 0, result.stderr
    if "help" in expected:
        actual = set(re.findall(r"^([a-z][a-z0-9-]*) - ", result.stdout, re.MULTILINE))
        assert actual == expected, (actual ^ expected, result.stdout)
    else:
        assert "unknown program: help" in result.stdout, result.stdout


commands = "help\nexit\n"
check(subprocess.run([sys.argv[1]], input=commands, text=True, capture_output=True, timeout=5))
with tempfile.TemporaryFile(mode="w+") as stream:
    stream.write(commands)
    stream.seek(0)
    check(subprocess.run([sys.argv[1]], stdin=stream, text=True, capture_output=True, timeout=5))

# The docs generator must capture complete help even for command names that
# share a prefix (gbemu-config / gbemu), without history suggestion escapes.
if "help" in expected:
    generator = runpy.run_path(str(Path(__file__).resolve().parents[1] / "tools/generate-command-docs.py"))
    docs = generator["query_host"](Path(sys.argv[1]))
    if "shell-config" in expected:
        assert "Usage:" in docs["shell-config"].help_text
    if "gbemu" in docs:
        assert "Usage:" in docs["gbemu"].help_text
    assert all("\x1b" not in doc.help_text for doc in docs.values())

# Exercise the alias through the real launcher: sharing only mv's handler loses
# its workspace quota and fails before reaching the filesystem.
if {"echo", "rename", "cat", "exit"} <= expected:
    # Several task launches and more than one input batch must preserve every
    # following command, both through a pipe and a redirected regular file.
    script = "".join(
        f"echo rename-workspace-{index} > /tmp/rename-source\n"
        "rename /tmp/rename-source /tmp/rename-target\n"
        "cat /tmp/rename-target\n"
        "rm /tmp/rename-target\n"
        for index in range(20)
    ) + "exit\n"

    def check_script(result):
        assert result.returncode == 0, result.stderr
        assert "arena is unavailable" not in result.stdout, result.stdout
        assert "unknown program:" not in result.stdout, result.stdout
        actual = re.findall(r"^rename-workspace-(\d+)\r?$", result.stdout, re.MULTILINE)
        assert actual == [str(index) for index in range(20)], result.stdout

    check_script(subprocess.run(
        [sys.argv[1]], input=script, text=True, capture_output=True, timeout=5,
    ))
    with tempfile.TemporaryFile(mode="w+") as stream:
        stream.write(script)
        stream.seek(0)
        check_script(subprocess.run(
            [sys.argv[1]], stdin=stream, text=True, capture_output=True, timeout=5,
        ))
