from __future__ import annotations

import os
import subprocess
import sys

import pytest


@pytest.mark.parametrize("package", ["lexxos_emu", "emulator.lexxos_emu"])
def test_package_imports_and_read_only_callback_outside_repo(package, repo_root, tmp_path):
    """Both entry points must catch the board's exception inside the C callback."""
    env = os.environ.copy()
    env["PYTHONPATH"] = str(repo_root / "emulator" if package == "lexxos_emu" else repo_root)
    result = subprocess.run(
        [sys.executable, "-c", f'''
import ctypes
import importlib
from pathlib import Path
package = importlib.import_module({package!r})
abi = importlib.import_module({package!r} + ".sim_abi")
image = Path("readonly.img")
image.write_bytes(bytes(512))
board = package.VirtualBoard.dev_pico(sd_image=image, sd_inserted=True)
bridge = abi.SimBoardApiBridge(board)
payload = (ctypes.c_uint8 * 512)()
assert bridge.api.block_write(None, 0, 0, payload, 1) == abi.LEXXOS_SIM_WRITE_PROTECTED
assert image.read_bytes() == bytes(512)
board.block(0).writable = True
board.block(0).fail_next_writes = 1
assert bridge.api.block_write(None, 0, 0, payload, 1) == -1
assert bridge.api.block_write(None, 0, 0, payload, 1) == 0
board.close()
'''],
        cwd=tmp_path,
        env=env,
        capture_output=True,
        text=True,
        timeout=10,
        check=False,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    assert "Exception ignored" not in result.stderr
