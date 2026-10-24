# Implementation notes

This directory is intentionally independent from the LexxOS firmware build.

Host side:

- `gui.py` contains the PySide6 file-manager UI.
- `serial_shell.py` wraps the LexxOS shell over UART.
- `ymodem.py` implements the small YMODEM/CRC subset used by `yrx` and `ysend`.

Firmware side expectations:

- LexxOS shell prompt format: `lexx:<cwd>> `.
- Directory listing through `ls <path>`.
- Upload command: `yrx <remote-path>`.
- Download command: `ysend <remote-path> <suggested-name>`.

The UI currently transfers one regular file at a time.  Directory recursion and multi-selection are intentionally left for a later iteration.
