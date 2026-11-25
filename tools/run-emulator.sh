#!/bin/sh
# Keep the Python environment and local SD contents out of the source tree.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export UV_PROJECT_ENVIRONMENT="$root/work/venvs/emulator"
exec uv run --project "$root/tools/emulator" python "$root/tools/emulator/lexxos_emu/gui.py" "$@"
