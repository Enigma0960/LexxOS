"""Serial shell adapter for LexxCP."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path, PurePosixPath
import re
import shlex
import time
from typing import Callable

import serial

from . import ymodem

PROMPT_RE = re.compile(rb"lexx:(?P<cwd>/[^\r\n>]*)> $")
ANSI_RE = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")


@dataclass(slots=True)
class RemoteEntry:
    name: str
    path: str
    is_dir: bool
    size: int | None = None


@dataclass(slots=True)
class CommandResult:
    output: str
    cwd: str


class ShellError(RuntimeError):
    pass


class LexxSerialClient:
    """Small synchronous client for the LexxOS UART shell."""

    def __init__(self, port: str, baudrate: int = 115200, timeout: float = 0.05):
        self.serial = serial.Serial(
            port=port,
            baudrate=baudrate,
            bytesize=8,
            parity="N",
            stopbits=1,
            timeout=timeout,
            write_timeout=2.0,
            xonxoff=False,
            rtscts=False,
            dsrdtr=False,
        )
        self.cwd = "/"

    def close(self) -> None:
        if self.serial.is_open:
            self.serial.close()

    def flush_input(self) -> None:
        self.serial.reset_input_buffer()

    def wait_for_prompt(self, timeout_s: float = 5.0) -> str:
        data = bytearray()
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            chunk = self.serial.read(256)
            if chunk:
                data.extend(chunk)
                match = PROMPT_RE.search(bytes(data))
                if match is not None:
                    self.cwd = match.group("cwd").decode("utf-8", "replace")
                    return self.cwd
            else:
                time.sleep(0.01)
        raise TimeoutError("LexxOS prompt was not detected")

    def command(self, command: str, timeout_s: float = 8.0) -> CommandResult:
        self.serial.reset_input_buffer()
        self.serial.write(command.encode("utf-8") + b"\r")
        data = bytearray()
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            chunk = self.serial.read(256)
            if chunk:
                data.extend(chunk)
                match = PROMPT_RE.search(bytes(data))
                if match is not None:
                    self.cwd = match.group("cwd").decode("utf-8", "replace")
                    output = self._strip_command_and_prompt(bytes(data), command)
                    return CommandResult(output=output, cwd=self.cwd)
            else:
                time.sleep(0.01)
        raise TimeoutError(f"command timed out: {command}")

    def list_dir(self, path: str) -> list[RemoteEntry]:
        result = self.command(f"ls {quote_remote(path)}")
        entries: list[RemoteEntry] = []
        for raw_line in result.output.splitlines():
            entry = parse_ls_line(path, raw_line)
            if entry is not None:
                entries.append(entry)
        entries.sort(key=lambda item: (not item.is_dir, item.name.lower()))
        return entries

    def upload(self, local_path: Path, remote_path: str, progress: Callable[[int, int], None] | None = None) -> None:
        self.serial.reset_input_buffer()
        self.serial.write(f"yrx {quote_remote(remote_path)}\r".encode("utf-8"))
        ymodem.send_file(self.serial, local_path, local_path.name, progress=progress)
        self.wait_for_prompt(timeout_s=10.0)

    def download(self, remote_path: str, local_path: Path, progress: Callable[[int, int], None] | None = None) -> Path:
        self.serial.reset_input_buffer()
        local_name = local_path.name
        self.serial.write(f"ysend {quote_remote(remote_path)} {quote_remote(local_name)}\r".encode("utf-8"))
        received = ymodem.receive_file(self.serial, local_path, progress=progress)
        self.wait_for_prompt(timeout_s=10.0)
        return received.path

    @staticmethod
    def _strip_command_and_prompt(data: bytes, command: str) -> str:
        text = data.decode("utf-8", "replace").replace("\r\n", "\n").replace("\r", "\n")
        lines = text.splitlines()
        if lines and lines[0].strip() == command.strip():
            lines = lines[1:]
        while lines and lines[-1].startswith("lexx:") and lines[-1].endswith("> "):
            lines = lines[:-1]
        return "\n".join(lines).strip("\n")


def quote_remote(path: str) -> str:
    return shlex.quote(path)


def join_remote(parent: str, child: str) -> str:
    if parent == "/":
        return f"/{child}"
    return str(PurePosixPath(parent) / child)


def parent_remote(path: str) -> str:
    if path == "/":
        return "/"
    parent = str(PurePosixPath(path).parent)
    return parent if parent else "/"


def parse_ls_line(parent: str, line: str) -> RemoteEntry | None:
    """Parse the human-readable LexxOS `ls` output.

    The current LexxOS `ls` command prints a PrettyTable with columns
    `type`, `name`, `size`, for example:

        | dir  | mnt | 0 |
        | file | a.bin | 123 |

    The parser also keeps a fallback for older/simple line formats.
    """

    line = ANSI_RE.sub("", line).strip()
    if not line or line.startswith("ls:") or line.startswith("total"):
        return None
    if line.startswith("+") or set(line) <= {"-", "+", "|", " "}:
        return None

    if line.startswith("|") and line.endswith("|"):
        cells = [cell.strip() for cell in line.strip("|").split("|")]
        if len(cells) < 3:
            return None
        type_name, name, size_text = cells[0], cells[1], cells[2]
        if type_name == "type" and name == "name":
            return None
        if type_name not in {"dir", "file", "block"}:
            return None
        try:
            size = int(size_text)
        except ValueError:
            size = None
        return RemoteEntry(name=name, path=join_remote(parent, name), is_dir=(type_name == "dir"), size=size)

    if line in {".", ".."}:
        return None

    parts = line.split()
    if not parts:
        return None

    # Non-directory target form from LexxOS: `file /path/to/file`.
    if parts[0] in {"dir", "file", "block"} and len(parts) >= 2:
        type_name = parts[0]
        name = PurePosixPath(parts[1]).name or parts[1]
        return RemoteEntry(name=name, path=join_remote(parent, name), is_dir=(type_name == "dir"), size=None)

    name = parts[-1]
    if name in {".", ".."}:
        return None

    lower_prefix = {part.lower() for part in parts[:-1]}
    is_dir = name.endswith("/") or "<dir>" in lower_prefix or "dir" in lower_prefix
    name = name.rstrip("/")
    size: int | None = None
    for part in parts[:-1]:
        if part.isdigit():
            size = int(part)
            break

    return RemoteEntry(name=name, path=join_remote(parent, name), is_dir=is_dir, size=size)
