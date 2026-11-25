from __future__ import annotations

from lexxos_emu.board import HostAudioPlayer, VirtualAudioSink


def test_gui_auto_audio_reports_missing_sounddevice(monkeypatch):
    monkeypatch.setattr(HostAudioPlayer, "_import_sounddevice", staticmethod(lambda: None))
    sink = VirtualAudioSink(output_enabled="auto")
    assert not sink.host_output_available
    assert "sounddevice" in (sink.host_output_error or "")
    assert sink.play_tone(mode=1, channel=2, frequency_hz=880, duration_ms=40, volume=120)
    assert len(sink.tone_requests) == 1


def test_standalone_tones_use_common_host_samplerate():
    player = HostAudioPlayer("off")
    assert player._active_or_default_sample_rate_hz() == 44100


def test_host_audio_stream_failures_are_reported(monkeypatch):
    class FailingSoundDevice:
        def RawOutputStream(self, **kwargs):
            raise RuntimeError("No valid output device")

    monkeypatch.setattr(HostAudioPlayer, "_import_sounddevice", staticmethod(lambda: FailingSoundDevice()))
    sink = VirtualAudioSink(output_enabled="auto")
    assert sink.host_output_available
    assert sink.play_tone(mode=1, channel=2, frequency_hz=1000, duration_ms=40, volume=128)
    assert not sink.host_output_available
    assert "No valid output device" in (sink.host_output_error or "")
