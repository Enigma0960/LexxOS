from __future__ import annotations

import sys
import wave
from types import SimpleNamespace

from emulator.lexxos_emu import VirtualAudioMode, VirtualBoard, VirtualI2CBus, normalize_display_size
from emulator.lexxos_emu.board import HostAudioPlayer


def test_virtual_i2c_bus_attach_detach_and_write():
    bus = VirtualI2CBus()

    assert not bus.probe(0x3C)
    assert not bus.write(0x3C, b"init")

    bus.attach(0xBC)

    assert bus.probe(0x3C)
    assert bus.attached_addresses() == (0x3C,)
    assert bus.write(0x3C, b"init")
    assert bus.writes == [(0x3C, b"init")]

    bus.detach(0x3C)

    assert not bus.probe(0x3C)


def test_dev_pico_can_expose_optional_oled_for_i2c_scan():
    board = VirtualBoard.dev_pico(oled=True)

    assert board.i2c_bus(0).probe(0x3C)
    assert board.i2c_bus(0).attached_addresses() == (0x3C,)


def test_dev_pico_keeps_display_size_and_audio_mode_config():
    board = VirtualBoard.dev_pico(display_size="480x272", audio_mode="adc")

    assert board.display_size == (480, 272)
    assert board.audio.mode == VirtualAudioMode.ADC


def test_host_audio_player_writes_single_wav_file(tmp_path):
    path = tmp_path / "lexxos-audio.wav"
    player = HostAudioPlayer(enabled="wav", wav_path=path)

    sample_rate_hz = 1000
    pcm = b"\x01\x00\x02\x00" * 4
    player.write_pcm_buffer(pcm, sample_rate_hz)

    assert player.drain_pcm_output(1000)
    assert path.exists()
    assert player.queued_pcm_chunks == 1
    assert player.completed_pcm_chunks == 1

    with wave.open(str(path), "rb") as wav:
        assert wav.getnchannels() == 2
        assert wav.getsampwidth() == 2
        assert wav.getframerate() == sample_rate_hz
        assert wav.getnframes() == 4
        assert wav.readframes(4) == pcm

def test_host_audio_player_prefers_sounddevice_stream(monkeypatch):
    streams = []

    class FakeRawOutputStream:
        def __init__(self, **kwargs):
            self.kwargs = kwargs
            self.callback = kwargs["callback"]
            self.started = False
            self.stopped = False
            self.closed = False
            streams.append(self)

        def start(self):
            self.started = True


        def stop(self):
            self.stopped = True

        def close(self):
            self.closed = True

    fake_sounddevice = SimpleNamespace(RawOutputStream=FakeRawOutputStream)
    monkeypatch.setitem(sys.modules, "sounddevice", fake_sounddevice)

    player = HostAudioPlayer(enabled=True)
    pcm = b"\x01\x00\x02\x00" * 100
    player.write_pcm_buffer(pcm, 1000)


    assert len(streams) == 1
    assert streams[0].started
    assert streams[0].kwargs["samplerate"] == 1000
    assert streams[0].kwargs["channels"] == 2
    assert streams[0].kwargs["dtype"] == "int16"
    assert player.buffered_pcm_frames == 100

    outdata = bytearray(len(pcm))
    streams[0].callback(outdata, 100, None, None)

    assert bytes(outdata) == pcm
    assert player.buffered_pcm_frames == 0
    assert player.drain_pcm_output(1000)
    assert streams[0].stopped
    assert streams[0].closed
    assert player.queued_pcm_chunks == 1
    assert player.completed_pcm_chunks >= 1


def test_display_size_validation():
    assert normalize_display_size("320x240") == (320, 240)
    try:
        normalize_display_size("0x240")
    except ValueError as exc:
        assert "display_size" in str(exc)
    else:
        raise AssertionError("display_size accepted a zero width")


def test_dev_pico_can_expose_host_directory_as_sd_card(tmp_path):
    sd_root = tmp_path / "sd-root"
    sd_root.mkdir()
    (sd_root / "README.TXT").write_text("Hello from host directory\n", encoding="utf-8")
    board = VirtualBoard.dev_pico(sd_directory=sd_root, sd_inserted=True)
    block = board.block(0)
    assert block.inserted
    assert block.writable
    assert block.source_directory == sd_root
    assert block.block_count > 0
    assert block.read_blocks(0, 1)[82:90] == b"FAT32   "
    assert block.block_count == (16 * 1024**3) // 512
    board.close()


def test_sd_image_and_host_directory_are_mutually_exclusive(tmp_path):
    image = tmp_path / "sd.img"
    image.write_bytes(bytes(512))
    directory = tmp_path / "sd-root"
    directory.mkdir()
    try:
        VirtualBoard.dev_pico(sd_image=image, sd_directory=directory)
    except ValueError as exc:
        assert "mutually exclusive" in str(exc)
    else:
        raise AssertionError("VirtualBoard accepted both sd_image and sd_directory")
