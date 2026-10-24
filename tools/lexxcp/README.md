# LexxCP

LexxCP is a small Python/PySide file manager for copying files between the host computer and LexxOS over the UART/USB-serial console.

The application drives the LexxOS shell commands added for UART file transfer:

- `yrx [path]` — receive a file on LexxOS from the host computer.
- `ysend <path> [name]` — send a LexxOS VFS file to the host computer.

The transfer protocol is YMODEM/CRC implemented in Python, so the host side does not require external `sb`/`rb` tools.

## Requirements

Python 3.10 or newer is recommended.

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

On Windows, activate the environment with:

```powershell
.venv\Scripts\Activate.ps1
pip install -r requirements.txt
```

## Run

```bash
python -m lexxcp
```

or from the repository root:

```bash
python tools/lexxcp/main.py
```

## Basic workflow

1. Build and flash LexxOS with the `yrx` and `ysend` commands.
2. Start LexxOS and wait for the shell prompt.
3. Start LexxCP on the host computer.
4. Select the serial port and baudrate.
5. Press **Connect**.
6. Use the left panel for the host filesystem and the right panel for the LexxOS VFS.
7. Use **Upload →** to copy a local file to LexxOS.
8. Use **← Download** to copy a LexxOS file to the host.

## Notes

The first version intentionally transfers one file at a time. Directory synchronization, batch transfer and resume can be added later on top of the same transport layer.

The LexxOS shell is text-oriented, while YMODEM transfers binary packets over the same UART stream. LexxCP therefore performs a strict command/transfer sequence: it sends a shell command, waits for the receiver/sender handshake byte, performs YMODEM, then waits for the LexxOS prompt again.
