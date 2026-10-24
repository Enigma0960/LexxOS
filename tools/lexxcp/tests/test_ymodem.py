from __future__ import annotations

from pathlib import Path

from lexxcp import ymodem


class ScriptedStream:
    def __init__(self, incoming: bytes = b"") -> None:
        self.incoming = bytearray(incoming)
        self.written = bytearray()

    def read(self, size: int = 1) -> bytes:
        if not self.incoming:
            return b""
        count = min(size, len(self.incoming))
        data = bytes(self.incoming[:count])
        del self.incoming[:count]
        return data

    def write(self, data: bytes) -> int:
        self.written.extend(data)
        return len(data)


def test_send_file_skips_shell_output_before_receiver_request(tmp_path: Path) -> None:
    source = tmp_path / "hello.txt"
    source.write_bytes(b"hello")

    stream = ScriptedStream(
        b"yrx /hello.txt\r\n"
        b"yrx: waiting for YMODEM sender\r\n"
        + bytes(
            (
                ymodem.CRCCHR,
            )
        )
        + b"M0 > /dev/ttyACM0\r\n"
        + bytes(
            (
                ymodem.ACK,
                ymodem.CRCCHR,
                ymodem.ACK,
                ymodem.ACK,
                ymodem.CRCCHR,
                ymodem.ACK,
            )
        )
    )

    ymodem.send_file(stream, source, timeout_s=0.01)

    assert stream.written[0] == ymodem.SOH
    assert bytes((ymodem.EOT,)) in bytes(stream.written)


def test_receive_file_skips_shell_output_before_metadata_packet(tmp_path: Path) -> None:
    metadata = ymodem._make_packet(0, ymodem._metadata_payload("hello.txt", 5))
    data = ymodem._make_packet(1, b"hello" + b"\x1A" * (ymodem.BLOCK_1024 - 5))
    empty = ymodem._make_packet(0, ymodem._empty_metadata_payload())
    stream = ScriptedStream(
        b"ysend /hello.txt hello.txt\r\n"
        b"ysend: waiting for YMODEM receiver\r\n"
        + metadata
        + data
        + bytes((ymodem.EOT,))
        + empty
    )

    received = ymodem.receive_file(stream, tmp_path / "downloaded.txt", timeout_s=0.01)

    assert received.path.read_bytes() == b"hello"
    assert received.name == "hello.txt"
    assert stream.written.startswith(bytes((ymodem.CRCCHR,)))
