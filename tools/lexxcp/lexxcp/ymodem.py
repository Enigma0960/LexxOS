"""Minimal YMODEM/CRC implementation used by LexxCP.

The implementation supports the single-file workflow used by the LexxOS
`yrx` and `ysend` commands: metadata block 0, 1024-byte data blocks,
CRC-16/CCITT, EOT finalization and optional empty block 0 at the end.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import time
from typing import Callable, Protocol

SOH = 0x01
STX = 0x02
EOT = 0x04
ACK = 0x06
NAK = 0x15
CAN = 0x18
CRCCHR = ord("C")

BLOCK_128 = 128
BLOCK_1024 = 1024
PACKET_RETRIES = 10


class CancelledTransfer(RuntimeError):
    """Raised when the remote side cancels a YMODEM transfer."""


class TransferError(RuntimeError):
    """Raised when a transfer cannot be completed."""


class ByteStream(Protocol):
    def read(self, size: int = 1) -> bytes:
        ...

    def write(self, data: bytes) -> int | None:
        ...


ProgressCallback = Callable[[int, int], None]


@dataclass(slots=True)
class ReceivedFile:
    name: str
    size: int
    path: Path


def crc16_ccitt(data: bytes) -> int:
    """Return CRC-16/CCITT compatible with X/YMODEM CRC mode."""

    crc = 0
    for value in data:
        crc ^= value << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def _read_exact(stream: ByteStream, size: int, timeout_s: float) -> bytes:
    deadline = time.monotonic() + timeout_s
    data = bytearray()
    while len(data) < size:
        if time.monotonic() >= deadline:
            raise TimeoutError("serial read timeout")
        chunk = stream.read(size - len(data))
        if chunk:
            data.extend(chunk)
        else:
            time.sleep(0.005)
    return bytes(data)


def _read_byte(stream: ByteStream, timeout_s: float) -> int:
    return _read_exact(stream, 1, timeout_s)[0]


def _write_byte(stream: ByteStream, value: int) -> None:
    stream.write(bytes((value,)))


def _cancel(stream: ByteStream) -> None:
    stream.write(bytes((CAN, CAN, CAN)))


def _wait_for_byte(
    stream: ByteStream,
    expected: int,
    timeout_s: float,
    retries: int,
    *,
    accepted: tuple[int, ...] = (),
    skip_noise: bool = False,
) -> int:
    if skip_noise:
        deadline = time.monotonic() + timeout_s * max(1, retries)
        last = None
        while time.monotonic() < deadline:
            remaining = max(0.0, deadline - time.monotonic())
            try:
                value = _read_byte(stream, min(timeout_s, remaining))
            except TimeoutError:
                continue
            last = value
            if value == CAN:
                raise CancelledTransfer("remote cancelled transfer")
            if value == expected or value in accepted:
                return value
        if last is None:
            raise TransferError(f"timed out waiting for 0x{expected:02X}")
        raise TransferError(f"expected 0x{expected:02X}, got 0x{last:02X}")

    last = None
    for _ in range(retries):
        try:
            value = _read_byte(stream, timeout_s)
        except TimeoutError:
            continue
        last = value
        if value == CAN:
            raise CancelledTransfer("remote cancelled transfer")
        if value == expected or value in accepted:
            return value
    if last is None:
        raise TransferError(f"timed out waiting for 0x{expected:02X}")
    raise TransferError(f"expected 0x{expected:02X}, got 0x{last:02X}")


def _make_packet(block_number: int, payload: bytes) -> bytes:
    if len(payload) == BLOCK_128:
        marker = SOH
    elif len(payload) == BLOCK_1024:
        marker = STX
    else:
        raise ValueError("payload must be 128 or 1024 bytes")

    block = block_number & 0xFF
    crc = crc16_ccitt(payload)
    return bytes((marker, block, 0xFF - block)) + payload + crc.to_bytes(2, "big")


def _metadata_payload(name: str, size: int) -> bytes:
    raw = f"{name}\0{size} ".encode("ascii", "replace")
    if len(raw) > BLOCK_128:
        raise TransferError("YMODEM metadata does not fit into block 0")
    return raw + b"\0" * (BLOCK_128 - len(raw))


def _empty_metadata_payload() -> bytes:
    return b"\0" * BLOCK_128


def _send_packet_with_ack(stream: ByteStream, packet: bytes, timeout_s: float, *, skip_noise: bool = False) -> None:
    for _ in range(PACKET_RETRIES):
        stream.write(packet)
        deadline = time.monotonic() + timeout_s
        while True:
            try:
                value = _read_byte(stream, max(0.0, deadline - time.monotonic()))
            except TimeoutError:
                break
            if value == ACK:
                return
            if value == CAN:
                raise CancelledTransfer("remote cancelled transfer")
            if value == NAK:
                break
            if not skip_noise:
                break
    raise TransferError("remote did not ACK packet")


def _read_packet_start(stream: ByteStream, timeout_s: float, *, skip_noise: bool = False) -> int:
    if not skip_noise:
        return _read_byte(stream, timeout_s)

    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        remaining = max(0.0, deadline - time.monotonic())
        value = _read_byte(stream, remaining)
        if value == CAN or value in (SOH, STX, EOT):
            return value
    raise TimeoutError("serial read timeout")


def _read_packet(stream: ByteStream, timeout_s: float, *, skip_noise: bool = False) -> tuple[int, bytes] | None:
    marker = _read_packet_start(stream, timeout_s, skip_noise=skip_noise)
    if marker == EOT:
        return None
    if marker == CAN:
        raise CancelledTransfer("remote cancelled transfer")
    if marker not in (SOH, STX):
        raise TransferError(f"unexpected YMODEM marker 0x{marker:02X}")

    size = BLOCK_128 if marker == SOH else BLOCK_1024
    header = _read_exact(stream, 2, timeout_s)
    block_number = header[0]
    block_inverse = header[1]
    if ((block_number + block_inverse) & 0xFF) != 0xFF:
        raise TransferError("invalid YMODEM block number complement")

    payload = _read_exact(stream, size, timeout_s)
    received_crc = int.from_bytes(_read_exact(stream, 2, timeout_s), "big")
    actual_crc = crc16_ccitt(payload)
    if received_crc != actual_crc:
        raise TransferError("invalid YMODEM packet CRC")

    return block_number, payload


def _parse_metadata(payload: bytes) -> tuple[str, int]:
    name_end = payload.find(b"\0")
    if name_end <= 0:
        return "", 0
    name = payload[:name_end].decode("utf-8", "replace")
    rest = payload[name_end + 1 :].split(b"\0", 1)[0].strip()
    size_text = rest.split(b" ", 1)[0] if rest else b"0"
    try:
        size = int(size_text.decode("ascii"))
    except ValueError as exc:
        raise TransferError("invalid YMODEM file size in metadata") from exc
    return name, size


def send_file(stream: ByteStream, source: Path, remote_name: str | None = None, *, timeout_s: float = 10.0, progress: ProgressCallback | None = None) -> None:
    """Send one local file through an already-open YMODEM stream."""

    source = source.expanduser().resolve()
    if not source.is_file():
        raise TransferError(f"not a regular file: {source}")

    name = remote_name or source.name
    size = source.stat().st_size

    _wait_for_byte(stream, CRCCHR, timeout_s, PACKET_RETRIES, skip_noise=True)
    _send_packet_with_ack(stream, _make_packet(0, _metadata_payload(name, size)), timeout_s, skip_noise=True)
    _wait_for_byte(stream, CRCCHR, timeout_s, PACKET_RETRIES)

    sent = 0
    block_number = 1
    with source.open("rb") as handle:
        while True:
            chunk = handle.read(BLOCK_1024)
            if not chunk:
                break
            if len(chunk) < BLOCK_1024:
                chunk += b"\x1A" * (BLOCK_1024 - len(chunk))
            _send_packet_with_ack(stream, _make_packet(block_number, chunk), timeout_s)
            sent = min(size, sent + BLOCK_1024)
            if progress is not None:
                progress(min(sent, size), size)
            block_number = (block_number + 1) & 0xFF

    for _ in range(PACKET_RETRIES):
        _write_byte(stream, EOT)
        try:
            value = _read_byte(stream, timeout_s)
        except TimeoutError:
            continue
        if value == ACK:
            break
        if value == CAN:
            raise CancelledTransfer("remote cancelled transfer")
    else:
        raise TransferError("remote did not ACK EOT")

    try:
        value = _read_byte(stream, 1.5)
    except TimeoutError:
        return

    if value == CRCCHR:
        _send_packet_with_ack(stream, _make_packet(0, _empty_metadata_payload()), timeout_s)
    elif value == CAN:
        raise CancelledTransfer("remote cancelled transfer")


def receive_file(stream: ByteStream, destination: Path, *, timeout_s: float = 10.0, progress: ProgressCallback | None = None) -> ReceivedFile:
    """Receive one file through an already-open YMODEM stream."""

    destination = destination.expanduser()
    destination_is_dir = destination.exists() and destination.is_dir()

    for _ in range(PACKET_RETRIES):
        _write_byte(stream, CRCCHR)
        try:
            packet = _read_packet(stream, timeout_s, skip_noise=True)
            break
        except TimeoutError:
            continue
        except TransferError:
            _write_byte(stream, NAK)
            continue
    else:
        _cancel(stream)
        raise TransferError("timed out waiting for YMODEM metadata")

    if packet is None:
        _write_byte(stream, ACK)
        raise TransferError("sender finished before metadata")

    block, payload = packet
    if block != 0:
        _cancel(stream)
        raise TransferError("first YMODEM packet is not metadata block 0")

    name, size = _parse_metadata(payload)
    if not name:
        _write_byte(stream, ACK)
        raise TransferError("empty YMODEM batch")

    target = destination / Path(name).name if destination_is_dir else destination
    target.parent.mkdir(parents=True, exist_ok=True)

    _write_byte(stream, ACK)
    _write_byte(stream, CRCCHR)

    received = 0
    expected_block = 1
    with target.open("wb") as handle:
        while True:
            try:
                packet = _read_packet(stream, timeout_s)
            except TimeoutError:
                _write_byte(stream, NAK)
                continue
            except TransferError:
                _write_byte(stream, NAK)
                continue

            if packet is None:
                _write_byte(stream, ACK)
                break

            block, payload = packet
            if block != (expected_block & 0xFF):
                _write_byte(stream, NAK)
                continue

            writable = min(len(payload), size - received)
            if writable > 0:
                handle.write(payload[:writable])
                received += writable
                if progress is not None:
                    progress(received, size)
            expected_block = (expected_block + 1) & 0xFF
            _write_byte(stream, ACK)

    _write_byte(stream, CRCCHR)
    try:
        packet = _read_packet(stream, 1.5)
    except (TimeoutError, TransferError):
        packet = None

    if packet is not None:
        _write_byte(stream, ACK)

    return ReceivedFile(name=name, size=size, path=target)
