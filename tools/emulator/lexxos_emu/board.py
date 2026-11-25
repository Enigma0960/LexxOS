from __future__ import annotations

import tempfile
import importlib
import math
import queue
import shutil
import subprocess
import sys
import threading
import time
import wave

from dataclasses import dataclass, field
from enum import IntEnum
from pathlib import Path
from typing import Dict, Iterable, Set

from .fat16_image import create_fat16_image_from_directory
from .fat32_image import create_fat32_image_from_directory
from .host_directory_sync import HostDirectorySync


class VirtualBlockDeviceError(RuntimeError):
    pass


class VirtualBlockDeviceWriteProtected(VirtualBlockDeviceError):
    pass


class VirtualAudioMode(IntEnum):
    OFF = 0
    PWM = 1
    ADC = 2


PCM_STREAM_CHUNK_SECONDS = 0.5


class _LegacyHostAudioPlayer:
    def __init__(self, enabled: bool | str = False):
        self.enabled = enabled != "off" and bool(enabled)
        self._sounddevice = self._import_sounddevice() if self.enabled else None
        self._command = self._find_command() if self.enabled else None
        self._pcm_stream_command = self._find_pcm_stream_command() if self.enabled else None
        self._pcm_lock = threading.Lock()
        self._pcm_done = threading.Condition()
        self._pcm_queue: queue.Queue[tuple[Path, float]] = queue.Queue()
        self._pcm_worker: threading.Thread | None = None
        self._sd_stream = None
        self._sd_sample_rate_hz = 0
        self._sd_failed = False
        self._last_error: str | None = None
        self._pcm_stream_process: subprocess.Popen | None = None
        self._pcm_stream_sample_rate_hz = 0
        self._pcm_path: Path | None = None
        self._pcm_writer: wave.Wave_write | None = None
        self._pcm_sample_rate_hz = 0
        self._pcm_frame_count = 0
        self._pcm_pending_chunks = 0
        self._pcm_queued_chunks = 0
        self._pcm_completed_chunks = 0
        if (
                enabled == "on"
                and self._sounddevice is None
                and self._command is None
                and self._pcm_stream_command is None
                and sys.platform != "win32"
        ):
            raise RuntimeError("host audio output requires sounddevice, ffplay, afplay, paplay or aplay")

    @property
    def available(self) -> bool:
        return self.enabled and (
                self._sounddevice is not None
                or self._command is not None
                or self._pcm_stream_command is not None
                or sys.platform == "win32"
        )

    @property
    def queued_pcm_chunks(self) -> int:
        with self._pcm_done:
            return self._pcm_queued_chunks

    @property
    def completed_pcm_chunks(self) -> int:
        with self._pcm_done:
            return self._pcm_completed_chunks

    @property
    def pcm_stream_enabled(self) -> bool:
        return self._sounddevice is not None or self._pcm_stream_command is not None

    @staticmethod
    def _import_sounddevice():
        try:
            return importlib.import_module("sounddevice")
        except Exception:
            return None

    @staticmethod
    def _find_command() -> str | None:
        for command in ("ffplay", "afplay", "paplay", "aplay"):
            path = shutil.which(command)
            if path:
                return path
        return None

    @staticmethod
    def _find_pcm_stream_command() -> str | None:
        for command in ("ffplay", "aplay", "paplay"):
            path = shutil.which(command)
            if path:
                return path
        return None

    def play_tone(self, frequency_hz: int, duration_ms: int, volume: int) -> None:
        if not self.available:
            return
        thread = threading.Thread(
            target=self._play_tone_blocking,
            args=(int(frequency_hz), int(duration_ms), int(volume)),
            daemon=True,
        )
        thread.start()

    def _play_tone_blocking(self, frequency_hz: int, duration_ms: int, volume: int) -> None:
        if sys.platform == "win32":
            try:
                import winsound
                winsound.Beep(max(37, min(32767, frequency_hz)), max(1, duration_ms))
            except RuntimeError:
                return
            return

        if self._command is None:
            return

        path = self._write_tone_wav(frequency_hz, duration_ms, volume)
        self._play_wav_file_blocking(path, max(1.0, duration_ms / 1000.0 + 2.0))

    def write_pcm_buffer(self, pcm_bytes: bytes, sample_rate_hz: int) -> None:
        if not self.available or not pcm_bytes or sample_rate_hz <= 0:
            return

        frame_count = len(pcm_bytes) // 4
        if self._write_sounddevice_pcm_buffer(pcm_bytes, sample_rate_hz):
            return
        if self._pcm_stream_command is not None and self._write_pcm_stream_buffer(pcm_bytes, sample_rate_hz):
            return

        with self._pcm_lock:
            if self._pcm_writer is not None and self._pcm_sample_rate_hz != sample_rate_hz:
                self._flush_pcm_locked(play=True)
            if self._pcm_writer is None:
                self._open_pcm_locked(sample_rate_hz)
            if self._pcm_writer is None:
                return
            self._pcm_writer.writeframesraw(pcm_bytes)
            self._pcm_frame_count += frame_count
            if self._pcm_frame_count >= self._pcm_chunk_frames(sample_rate_hz):
                self._flush_pcm_locked(play=True)
        self._pace_pcm_output(frame_count / sample_rate_hz)

    def drain_pcm_output(self, timeout_ms: int) -> bool:
        if not self.available:
            return True
        sounddevice_drained = self._drain_sounddevice_pcm()
        stream_drained = self._close_pcm_stream(wait=True, timeout_ms=timeout_ms)
        with self._pcm_lock:
            self._flush_pcm_locked(play=True, extra_timeout_ms=timeout_ms)
        return sounddevice_drained and stream_drained and self._wait_for_pcm_playback(timeout_ms)

    def stop_pcm_output(self) -> None:
        self._stop_sounddevice_pcm(abort=True)
        self._close_pcm_stream(wait=False)
        with self._pcm_lock:
            self._flush_pcm_locked(play=False)
        self._discard_queued_pcm()

    def _write_sounddevice_pcm_buffer(self, pcm_bytes: bytes, sample_rate_hz: int) -> bool:
        with self._pcm_lock:
            if not self._ensure_sounddevice_stream_locked(sample_rate_hz):
                return False
            try:
                self._sd_stream.write(pcm_bytes)
            except Exception:
                self._close_sounddevice_stream_locked(abort=True)
                self._sd_failed = True
                return False
        return True

    def _ensure_sounddevice_stream_locked(self, sample_rate_hz: int) -> bool:
        if self._sounddevice is None or self._sd_failed:
            return False
        if self._sd_stream is not None and self._sd_sample_rate_hz == sample_rate_hz:
            return True

        self._close_sounddevice_stream_locked(abort=False)
        try:
            self._sd_stream = self._sounddevice.RawOutputStream(
                samplerate=sample_rate_hz,
                channels=2,
                dtype="int16",
                blocksize=0,
                latency="low",
            )
            self._sd_stream.start()
        except Exception:
            self._sd_stream = None
            self._sd_sample_rate_hz = 0
            self._sd_failed = True
            return False

        self._sd_sample_rate_hz = sample_rate_hz
        return True

    def _drain_sounddevice_pcm(self) -> bool:
        with self._pcm_lock:
            return self._close_sounddevice_stream_locked(abort=False)

    def _stop_sounddevice_pcm(self, *, abort: bool) -> None:
        with self._pcm_lock:
            self._close_sounddevice_stream_locked(abort=abort)

    def _close_sounddevice_stream_locked(self, *, abort: bool) -> bool:
        stream = self._sd_stream
        self._sd_stream = None
        self._sd_sample_rate_hz = 0
        if stream is None:
            return True

        try:
            if abort and hasattr(stream, "abort"):
                stream.abort()
            else:
                stream.stop()
            stream.close()
            return True
        except Exception:
            return False

    def _write_pcm_stream_buffer(self, pcm_bytes: bytes, sample_rate_hz: int) -> bool:
        with self._pcm_lock:
            if not self._ensure_pcm_stream_locked(sample_rate_hz):
                return False
            process = self._pcm_stream_process

            if process is None or process.stdin is None:
                return False
            try:
                process.stdin.write(pcm_bytes)
            except (BrokenPipeError, OSError, TypeError):
                self._close_pcm_stream_locked(wait=False)
                return False
        return True

    def _ensure_pcm_stream_locked(self, sample_rate_hz: int) -> bool:
        process = self._pcm_stream_process
        if (
                process is not None
                and process.poll() is None
                and self._pcm_stream_sample_rate_hz == sample_rate_hz
                and process.stdin is not None
        ):
            return True

        self._close_pcm_stream_locked(wait=True, timeout_ms=1000)
        if self._pcm_stream_command is None:
            return False

        try:
            self._pcm_stream_process = subprocess.Popen(
                self._pcm_stream_args(sample_rate_hz),
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                shell=False,
                text=False,
                bufsize=0,
            )
        except OSError:
            self._pcm_stream_process = None
            self._pcm_stream_sample_rate_hz = 0
            return False
        self._pcm_stream_sample_rate_hz = sample_rate_hz
        if self._pcm_stream_process.stdin is None:
            if self._pcm_stream_process.poll() is None:
                self._pcm_stream_process.terminate()
            self._pcm_stream_process = None
            self._pcm_stream_sample_rate_hz = 0
            return False
        return True

    def _close_pcm_stream(self, *, wait: bool, timeout_ms: int = 0) -> bool:
        with self._pcm_lock:
            return self._close_pcm_stream_locked(wait=wait, timeout_ms=timeout_ms)

    def _close_pcm_stream_locked(self, *, wait: bool, timeout_ms: int = 0) -> bool:
        process = self._pcm_stream_process
        self._pcm_stream_process = None
        self._pcm_stream_sample_rate_hz = 0
        if process is None:
            return True

        if process.stdin is not None:
            try:
                process.stdin.close()
            except OSError:
                pass

        if not wait:
            if process.poll() is None:
                process.terminate()
            return True

        timeout_s = max(0.1, timeout_ms / 1000.0 if timeout_ms > 0 else 1.0)
        try:
            process.wait(timeout=timeout_s)
            return True
        except subprocess.TimeoutExpired:
            process.terminate()
            try:
                process.wait(timeout=1.0)
            except subprocess.TimeoutExpired:
                process.kill()
            return False

    def _pcm_stream_args(self, sample_rate_hz: int) -> list[str]:
        command_name = Path(self._pcm_stream_command or "").name
        if command_name == "ffplay":
            return [
                self._pcm_stream_command or "ffplay",
                "-hide_banner",
                "-loglevel",
                "error",
                "-nodisp",
                "-autoexit",
                # "-fflags",
                # "nobuffer",
                # "-flags",
                # "low_delay",
                "-f",
                "s16le",
                "-ac",
                "2",
                "-ar",
                str(sample_rate_hz),
                "-i",
                "pipe:0",
            ]
        if command_name == "aplay":
            return [
                self._pcm_stream_command or "aplay",
                "-q",
                "-f",
                "S16_LE",
                "-c",
                "2",
                "-r",
                str(sample_rate_hz),
            ]
        if command_name == "paplay":
            return [
                self._pcm_stream_command or "paplay",
                "--raw",
                "--format=s16le",
                "--channels=2",
                f"--rate={sample_rate_hz}",
            ]
        return [self._pcm_stream_command or command_name]

    def _open_pcm_locked(self, sample_rate_hz: int) -> None:
        handle = tempfile.NamedTemporaryFile(prefix="lexxos-pcm-", suffix=".wav", delete=False)
        self._pcm_path = Path(handle.name)
        handle.close()
        self._pcm_writer = wave.open(str(self._pcm_path), "wb")
        self._pcm_writer.setnchannels(2)
        self._pcm_writer.setsampwidth(2)
        self._pcm_writer.setframerate(sample_rate_hz)
        self._pcm_sample_rate_hz = sample_rate_hz
        self._pcm_frame_count = 0

    def _flush_pcm_locked(self, *, play: bool, extra_timeout_ms: int = 0) -> None:
        writer = self._pcm_writer
        path = self._pcm_path
        sample_rate_hz = self._pcm_sample_rate_hz
        frame_count = self._pcm_frame_count
        self._pcm_writer = None
        self._pcm_path = None
        self._pcm_sample_rate_hz = 0
        self._pcm_frame_count = 0

        if writer is not None:
            writer.close()
        if path is None:
            return
        if not play or frame_count == 0:
            self._unlink_quietly(path)
            return

        duration_s = frame_count / sample_rate_hz if sample_rate_hz > 0 else 0.0
        timeout_s = max(1.0, duration_s + extra_timeout_ms / 1000.0 + 2.0)
        self._queue_wav_file(path, timeout_s)

    @staticmethod
    def _pcm_chunk_frames(sample_rate_hz: int) -> int:
        return max(1, int(sample_rate_hz * PCM_STREAM_CHUNK_SECONDS))

    @staticmethod
    def _pace_pcm_output(duration_s: float) -> None:
        if duration_s > 0:
            time.sleep(duration_s)

    def _queue_wav_file(self, path: Path, timeout_s: float) -> None:
        with self._pcm_done:
            self._pcm_pending_chunks += 1
            self._pcm_queued_chunks += 1
            if self._pcm_worker is None or not self._pcm_worker.is_alive():
                self._pcm_worker = threading.Thread(target=self._pcm_worker_loop, daemon=True)
                self._pcm_worker.start()
        self._pcm_queue.put((path, timeout_s))

    def _pcm_worker_loop(self) -> None:
        while True:
            path, timeout_s = self._pcm_queue.get()
            try:
                self._play_wav_file_blocking(path, timeout_s)
            finally:
                self._pcm_queue.task_done()
                with self._pcm_done:
                    self._pcm_pending_chunks = max(0, self._pcm_pending_chunks - 1)
                    self._pcm_completed_chunks += 1
                    self._pcm_done.notify_all()

    def _wait_for_pcm_playback(self, timeout_ms: int) -> bool:
        deadline = time.monotonic() + timeout_ms / 1000.0 if timeout_ms > 0 else None
        with self._pcm_done:
            while self._pcm_pending_chunks > 0:
                if deadline is None:
                    self._pcm_done.wait(0.05)
                    continue
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return False
                self._pcm_done.wait(min(remaining, 0.05))
            return True

    def _discard_queued_pcm(self) -> None:
        discarded = 0
        while True:
            try:
                path, _timeout_s = self._pcm_queue.get_nowait()
            except queue.Empty:
                break
            self._unlink_quietly(path)
            self._pcm_queue.task_done()
            discarded += 1
        if discarded:
            with self._pcm_done:
                self._pcm_pending_chunks = max(0, self._pcm_pending_chunks - discarded)
                self._pcm_done.notify_all()

    def _play_wav_file_blocking(self, path: Path, timeout_s: float) -> None:
        try:
            if sys.platform == "win32":
                try:
                    import winsound

                    winsound.PlaySound(str(path), winsound.SND_FILENAME)
                except RuntimeError:
                    pass
                return

            if self._command is None:
                return
            subprocess.run(
                [self._command, str(path)],
                stdin=subprocess.PIPE,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=timeout_s,
                check=False,
            )
        except (OSError, subprocess.SubprocessError):
            pass
        finally:
            self._unlink_quietly(path)

    @staticmethod
    def _unlink_quietly(path: Path) -> None:
        try:
            path.unlink()
        except OSError:
            pass

    @staticmethod
    def _write_tone_wav(frequency_hz: int, duration_ms: int, volume: int) -> Path:
        sample_rate = 44100
        sample_count = max(1, int(sample_rate * max(1, duration_ms) / 1000))
        amplitude = int(32767 * 0.35 * max(0, min(255, volume)) / 255)
        frequency = max(20, min(20000, frequency_hz))
        handle = tempfile.NamedTemporaryFile(prefix="lexxos-tone-", suffix=".wav", delete=False)
        path = Path(handle.name)
        handle.close()
        with wave.open(str(path), "wb") as wav:
            wav.setnchannels(1)
            wav.setsampwidth(2)
            wav.setframerate(sample_rate)
            frames = bytearray()
            for index in range(sample_count):
                fade = min(index / 128.0, (sample_count - index - 1) / 128.0, 1.0)
                sample = int(math.sin(2.0 * math.pi * frequency * index / sample_rate) * amplitude * max(0.0, fade))
                frames.extend(int(sample).to_bytes(2, "little", signed=True))
            wav.writeframes(bytes(frames))
        return path


PCM_BYTES_PER_FRAME = 4  # s16le stereo: 2 channels * 2 bytes.
DEFAULT_AUDIO_WAV_PATH = Path("lexxos-audio.wav")
AUDIO_DRAIN_POLL_SECONDS = 0.01


class HostAudioPlayer:
    """Host audio backend based on one continuous sounddevice callback stream.

    `write_pcm_buffer()` only appends PCM bytes to an in-memory buffer. The
    sounddevice callback consumes that buffer using the host audio clock, so the
    LexxOS renderer can produce MIDI chunks in bursts without starting a new
    process or a new playback job per chunk.

    `--audio-output wav` uses the same PCM input path but writes one WAV file
    instead of opening a live playback stream.
    """

    def __init__(self, enabled: bool | str = False, *, wav_path: str | Path | None = None):
        self.output_mode = self._normalize_output_mode(enabled)
        self.enabled = self.output_mode != "off"
        self._sounddevice = self._import_sounddevice() if self.output_mode in ("auto", "on") else None

        self._pcm_lock = threading.RLock()
        self._pcm_available = threading.Condition(self._pcm_lock)

        self._sd_stream = None
        self._sd_sample_rate_hz = 0
        self._sd_failed = False
        self._sd_buffer = bytearray()
        self._sd_buffered_frames = 0
        self._sd_underflow_count = 0

        self._wav_path = Path(wav_path) if wav_path is not None else DEFAULT_AUDIO_WAV_PATH
        self._wav_writer: wave.Wave_write | None = None
        self._wav_sample_rate_hz = 0
        self._wav_frame_count = 0

        self._pcm_queued_chunks = 0
        self._pcm_completed_chunks = 0

        if self._sounddevice is None and self.output_mode in ("auto", "on"):
            self._last_error = "install sounddevice in the emulator's Python environment"

        if self.output_mode == "on" and self._sounddevice is None:
            raise RuntimeError("host audio output requires python package sounddevice")

    @property
    def last_error(self) -> str | None:
        return self._last_error

    @property
    def available(self) -> bool:
        if self.output_mode == "wav":
            return True
        return self.output_mode in ("auto", "on") and self._sounddevice is not None and not self._sd_failed

    @property
    def queued_pcm_chunks(self) -> int:
        with self._pcm_lock:
            return self._pcm_queued_chunks

    @property
    def completed_pcm_chunks(self) -> int:
        with self._pcm_lock:
            return self._pcm_completed_chunks

    @property
    def pcm_stream_enabled(self) -> bool:
        return self.output_mode in ("auto", "on") and self._sounddevice is not None and not self._sd_failed

    @property
    def buffered_pcm_frames(self) -> int:
        with self._pcm_lock:
            return self._sd_buffered_frames

    @property
    def pcm_underflow_count(self) -> int:
        with self._pcm_lock:
            return self._sd_underflow_count

    @staticmethod
    def _normalize_output_mode(enabled: bool | str) -> str:
        if isinstance(enabled, str):
            normalized = enabled.strip().lower().replace("-", "_")
            aliases = {
                "": "off",
                "0": "off",
                "false": "off",
                "no": "off",
                "off": "off",
                "none": "off",
                "disabled": "off",
                "1": "on",
                "true": "on",
                "yes": "on",
                "on": "on",
                "auto": "auto",
                "wav": "wav",
                "wave": "wav",
                "file": "wav",
            }
            try:
                return aliases[normalized]
            except KeyError as exc:
                raise ValueError("audio output must be auto, on, off or wav") from exc
        return "on" if enabled else "off"

    @staticmethod
    def _import_sounddevice():
        try:
            return importlib.import_module("sounddevice")
        except Exception:
            return None

    def play_tone(self, frequency_hz: int, duration_ms: int, volume: int) -> None:
        if not self.available:
            return
        sample_rate_hz = self._active_or_default_sample_rate_hz()
        self.write_pcm_buffer(
            self._build_tone_pcm(frequency_hz, duration_ms, volume, sample_rate_hz),
            sample_rate_hz,
        )

    def write_pcm_buffer(self, pcm_bytes: bytes, sample_rate_hz: int) -> None:
        if not self.available or not pcm_bytes or sample_rate_hz <= 0:
            return

        frame_count = len(pcm_bytes) // PCM_BYTES_PER_FRAME
        if frame_count <= 0:
            return

        if self.output_mode == "wav":
            self._write_wav_pcm_buffer(pcm_bytes, sample_rate_hz, frame_count)
            return

        self._queue_sounddevice_pcm_buffer(pcm_bytes, sample_rate_hz, frame_count)

    def drain_pcm_output(self, timeout_ms: int) -> bool:
        if not self.available:
            return True
        if self.output_mode == "wav":
            return self._close_wav_writer()
        return self._drain_sounddevice_pcm(timeout_ms)

    def stop_pcm_output(self) -> None:
        self._stop_sounddevice_pcm(abort=True)
        self._close_wav_writer()

    def _active_or_default_sample_rate_hz(self) -> int:
        with self._pcm_lock:
            if self.output_mode == "wav" and self._wav_sample_rate_hz > 0:
                return self._wav_sample_rate_hz
            if self._sd_sample_rate_hz > 0:
                return self._sd_sample_rate_hz
        # Mac/CoreAudio devices are more likely to accept this for standalone tones.
        return 44100

    def _queue_sounddevice_pcm_buffer(self, pcm_bytes: bytes, sample_rate_hz: int, frame_count: int) -> None:
        with self._pcm_lock:
            if not self._prepare_sounddevice_stream_locked(sample_rate_hz):
                return
            self._sd_buffer.extend(pcm_bytes)
            self._sd_buffered_frames += frame_count
            self._pcm_queued_chunks += 1
            self._pcm_available.notify_all()

    def _prepare_sounddevice_stream_locked(self, sample_rate_hz: int) -> bool:
        if self._sounddevice is None or self._sd_failed:
            return False
        if self._sd_stream is not None and self._sd_sample_rate_hz == sample_rate_hz:
            return True
        if self._sd_stream is not None:
            self._close_sounddevice_stream_locked(abort=False)
            self._sd_buffer.clear()
            self._sd_buffered_frames = 0

        try:
            self._sd_stream = self._sounddevice.RawOutputStream(
                samplerate=sample_rate_hz,
                channels=2,
                dtype="int16",
                blocksize=512,
                latency="high",
                callback=self._sounddevice_callback,
            )
            self._sd_stream.start()
        except Exception as exc:
            self._last_error = str(exc)
            self._sd_stream = None
            self._sd_sample_rate_hz = 0
            self._sd_failed = True
            self._sd_buffer.clear()
            self._sd_buffered_frames = 0
            return False

        self._sd_sample_rate_hz = sample_rate_hz
        return True

    def _sounddevice_callback(self, outdata, frames: int, _time_info, _status) -> None:
        required_bytes = max(0, int(frames)) * PCM_BYTES_PER_FRAME
        if required_bytes <= 0:
            return

        with self._pcm_lock:
            available_bytes = min(required_bytes, len(self._sd_buffer))
            if available_bytes:
                outdata[:available_bytes] = self._sd_buffer[:available_bytes]
                del self._sd_buffer[:available_bytes]
                consumed_frames = available_bytes // PCM_BYTES_PER_FRAME
                self._sd_buffered_frames = max(0, self._sd_buffered_frames - consumed_frames)
                self._pcm_completed_chunks += 1

            if available_bytes < required_bytes:
                outdata[available_bytes:required_bytes] = b"\x00" * (required_bytes - available_bytes)
                self._sd_underflow_count += 1

            self._pcm_available.notify_all()

    def _drain_sounddevice_pcm(self, timeout_ms: int) -> bool:
        deadline = time.monotonic() + timeout_ms / 1000.0 if timeout_ms > 0 else None
        with self._pcm_lock:
            while self._sd_buffered_frames > 0:
                if deadline is None:
                    self._pcm_available.wait(AUDIO_DRAIN_POLL_SECONDS)
                    continue
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return False
                self._pcm_available.wait(min(remaining, AUDIO_DRAIN_POLL_SECONDS))
            return self._close_sounddevice_stream_locked(abort=False)

    def _stop_sounddevice_pcm(self, *, abort: bool) -> None:
        with self._pcm_lock:
            self._sd_buffer.clear()
            self._sd_buffered_frames = 0
            self._pcm_available.notify_all()
            self._close_sounddevice_stream_locked(abort=abort)

    def _close_sounddevice_stream_locked(self, *, abort: bool) -> bool:
        stream = self._sd_stream
        self._sd_stream = None
        self._sd_sample_rate_hz = 0
        self._sd_buffer.clear()
        self._sd_buffered_frames = 0
        if stream is None:
            return True

        try:
            if abort and hasattr(stream, "abort"):
                stream.abort()
            else:
                stream.stop()
            stream.close()
            return True
        except Exception:
            return False

    def _write_wav_pcm_buffer(self, pcm_bytes: bytes, sample_rate_hz: int, frame_count: int) -> None:
        with self._pcm_lock:
            if self._wav_writer is not None and self._wav_sample_rate_hz != sample_rate_hz:
                self._close_wav_writer_locked()
            if self._wav_writer is None:
                self._open_wav_writer_locked(sample_rate_hz)
            if self._wav_writer is None:
                return
            self._wav_writer.writeframesraw(pcm_bytes)
            self._wav_frame_count += frame_count
            self._pcm_queued_chunks += 1

    def _open_wav_writer_locked(self, sample_rate_hz: int) -> None:
        self._wav_path.parent.mkdir(parents=True, exist_ok=True)
        self._wav_writer = wave.open(str(self._wav_path), "wb")
        self._wav_writer.setnchannels(2)
        self._wav_writer.setsampwidth(2)
        self._wav_writer.setframerate(sample_rate_hz)
        self._wav_sample_rate_hz = sample_rate_hz
        self._wav_frame_count = 0

    def _close_wav_writer(self) -> bool:
        with self._pcm_lock:
            return self._close_wav_writer_locked()

    def _close_wav_writer_locked(self) -> bool:
        writer = self._wav_writer
        self._wav_writer = None
        self._wav_sample_rate_hz = 0
        self._wav_frame_count = 0
        if writer is None:
            return True
        try:
            writer.close()
            self._pcm_completed_chunks = self._pcm_queued_chunks
            return True
        except OSError:
            return False

    @staticmethod
    def _build_tone_pcm(frequency_hz: int, duration_ms: int, volume: int, sample_rate_hz: int) -> bytes:
        sample_count = max(1, int(sample_rate_hz * max(1, duration_ms) / 1000))
        amplitude = int(32767 * 0.35 * max(0, min(255, volume)) / 255)
        frequency = max(20, min(20000, frequency_hz))
        frames = bytearray()
        for index in range(sample_count):
            fade = min(index / 128.0, (sample_count - index - 1) / 128.0, 1.0)
            sample = int(math.sin(2.0 * math.pi * frequency * index / sample_rate_hz) * amplitude * max(0.0, fade))
            encoded = int(sample).to_bytes(2, "little", signed=True)
            frames.extend(encoded)
            frames.extend(encoded)
        return bytes(frames)


DEFAULT_DISPLAY_SIZE = (320, 320)
MIN_DISPLAY_DIMENSION = 1
MAX_DISPLAY_DIMENSION = 4096
MAX_DISPLAY_PIXELS = 16 * 1024 * 1024


def normalize_display_size(display_size: str | Iterable[int] | None) -> tuple[int, int]:
    if display_size is None:
        return DEFAULT_DISPLAY_SIZE
    if isinstance(display_size, str):
        parts = display_size.lower().replace(",", "x").split("x")
        if len(parts) != 2:
            raise ValueError("display_size must use WIDTHxHEIGHT")
        width, height = (int(part.strip(), 10) for part in parts)
    else:
        values = tuple(display_size)
        if len(values) != 2:
            raise ValueError("display_size must contain width and height")
        width, height = int(values[0]), int(values[1])

    if (
            width < MIN_DISPLAY_DIMENSION
            or height < MIN_DISPLAY_DIMENSION
            or width > MAX_DISPLAY_DIMENSION
            or height > MAX_DISPLAY_DIMENSION
            or width * height > MAX_DISPLAY_PIXELS
    ):
        raise ValueError(
            f"display_size must be {MIN_DISPLAY_DIMENSION}..{MAX_DISPLAY_DIMENSION} per axis "
            f"and no more than {MAX_DISPLAY_PIXELS} pixels"
        )
    return width, height


def normalize_audio_mode(audio_mode: str | int | VirtualAudioMode | None) -> VirtualAudioMode:
    if audio_mode is None:
        return VirtualAudioMode.OFF
    if isinstance(audio_mode, VirtualAudioMode):
        return audio_mode
    if isinstance(audio_mode, str):
        normalized = audio_mode.strip().lower().replace("-", "_")
        aliases = {
            "off": VirtualAudioMode.OFF,
            "none": VirtualAudioMode.OFF,
            "disabled": VirtualAudioMode.OFF,
            "pwm": VirtualAudioMode.PWM,
            "adc": VirtualAudioMode.ADC,
        }
        try:
            return aliases[normalized]
        except KeyError as exc:
            raise ValueError("audio_mode must be off, pwm or adc") from exc
    try:
        return VirtualAudioMode(int(audio_mode))
    except ValueError as exc:
        raise ValueError("audio_mode must be off, pwm or adc") from exc


@dataclass(frozen=True)
class VirtualAudioToneRequest:
    mode: VirtualAudioMode
    channel: int
    frequency_hz: int
    duration_ms: int
    volume: int


@dataclass(frozen=True)
class VirtualAudioPcmBuffer:
    mode: VirtualAudioMode
    channel: int
    frame_count: int
    sample_rate_hz: int
    peak_left: int
    peak_right: int


@dataclass
class VirtualAudioSink:
    mode: VirtualAudioMode = VirtualAudioMode.PWM
    output_enabled: bool | str = False
    output_wav_path: str | Path | None = None
    tone_requests: list[VirtualAudioToneRequest] = field(default_factory=list)
    pcm_buffers: list[VirtualAudioPcmBuffer] = field(default_factory=list)
    drain_count: int = 0
    stop_count: int = 0
    _host_player: HostAudioPlayer = field(init=False, repr=False)

    def __post_init__(self) -> None:
        self._host_player = HostAudioPlayer(self.output_enabled, wav_path=self.output_wav_path)

    @property
    def host_output_available(self) -> bool:
        return self._host_player.available

    @property
    def host_output_error(self) -> str | None:
        return self._host_player.last_error

    def play_tone(
            self,
            *,
            mode: int,
            channel: int,
            frequency_hz: int,
            duration_ms: int,
            volume: int,
    ) -> bool:
        resolved_mode = normalize_audio_mode(mode)
        if resolved_mode == VirtualAudioMode.OFF:
            return False
        self.tone_requests.append(
            VirtualAudioToneRequest(
                mode=resolved_mode,
                channel=int(channel),
                frequency_hz=int(frequency_hz),
                duration_ms=int(duration_ms),
                volume=int(volume),
            )
        )
        self._host_player.play_tone(frequency_hz, duration_ms, volume)
        return True

    def play_pcm_buffer(
            self,
            *,
            mode: int,
            channel: int,
            frame_count: int,
            sample_rate_hz: int,
            peak_left: int,
            peak_right: int,
            pcm_bytes: bytes | None = None,
    ) -> bool:
        resolved_mode = normalize_audio_mode(mode)
        if resolved_mode == VirtualAudioMode.OFF:
            return False
        self.pcm_buffers.append(
            VirtualAudioPcmBuffer(
                mode=resolved_mode,
                channel=int(channel),
                frame_count=int(frame_count),
                sample_rate_hz=int(sample_rate_hz),
                peak_left=int(peak_left),
                peak_right=int(peak_right),
            )
        )
        if pcm_bytes:
            self._host_player.write_pcm_buffer(bytes(pcm_bytes), int(sample_rate_hz))
        return True

    def drain(self, *, timeout_ms: int = 0) -> bool:
        self.drain_count += 1
        return self._host_player.drain_pcm_output(timeout_ms)

    def stop(self) -> None:
        self.stop_count += 1
        self._host_player.stop_pcm_output()


@dataclass
class VirtualClock:
    now_us: int = 0

    def advance_ms(self, milliseconds: int) -> None:
        self.now_us += int(milliseconds) * 1000


@dataclass
class VirtualBlockDevice:
    image_path: Path | None = None
    inserted: bool = False
    writable: bool = False
    block_size: int = 512
    fail_next_reads: int = 0
    fail_next_writes: int = 0
    source_directory: Path | None = None

    _temporary_directory: tempfile.TemporaryDirectory[str] | None = field(default=None, init=False, repr=False)
    _directory_mirror: HostDirectorySync | None = field(default=None, init=False, repr=False)

    def attach_image(self, image_path: str | Path, *, inserted: bool = True, writable: bool = False) -> None:
        self.close()
        self.image_path = Path(image_path)
        self.source_directory = None
        self.inserted = inserted
        self.writable = writable

    def attach_directory(self, source_directory: str | Path, *, inserted: bool = True,
                         writable: bool = True, capacity: str = "16G",
                         filesystem: str = "fat32") -> None:
        self.close()
        directory = Path(source_directory)
        if not directory.exists():
            raise FileNotFoundError(directory)
        if not directory.is_dir():
            raise NotADirectoryError(directory)
        if filesystem == "fat16" and capacity not in ("256M", "512M"):
            raise ValueError("FAT16 is available only with 256M or 512M capacity")
        if filesystem not in ("fat16", "fat32"):
            raise ValueError(f"unsupported SD filesystem: {filesystem}")

        self._temporary_directory = tempfile.TemporaryDirectory(prefix="lexxos-sd-")
        image_path = Path(self._temporary_directory.name) / "host-directory-sd.img"

        try:
            if filesystem == "fat32":
                create_fat32_image_from_directory(directory, image_path, capacity=capacity)
            else:
                mib = int(capacity[:-1])
                create_fat16_image_from_directory(
                    directory, image_path, size_mib=mib,
                    sectors_per_cluster=16 if mib == 256 else 32,
                    max_file_size=0xFFFFFFFF,
                )
            if filesystem == "fat32":
                self._directory_mirror = HostDirectorySync(directory, image_path, capacity=capacity)
        except BaseException:
            self._temporary_directory.cleanup()
            self._temporary_directory = None
            raise

        self.image_path = image_path
        self.source_directory = directory
        self.inserted = inserted
        self.writable = writable

    def close(self) -> None:
        if self._temporary_directory is not None:
            self._temporary_directory.cleanup()
            self._temporary_directory = None
        if self.source_directory is not None:
            self.source_directory = None
            self.image_path = None

    def sync_directory(self, stage: int) -> int:
        """Simulator's sync command handshake; only FAT32 --sd-dir mirrors."""
        mirror = self._directory_mirror
        if mirror is None:
            return 0
        if stage == 0:
            return mirror.preflight()
        if stage == 1:
            mirror.apply()
            return 0
        if stage == 2:
            mirror.cancel()
            return 0
        raise ValueError(f"unsupported directory sync stage: {stage}")

    def insert(self) -> None:
        self.inserted = True

    def remove(self) -> None:
        self.inserted = False

    @property
    def block_count(self) -> int:
        if not self.inserted or self.image_path is None or not self.image_path.exists():
            return 0
        return self.image_path.stat().st_size // self.block_size

    def read_blocks(self, first_block: int, block_count: int) -> bytes:
        if not self.inserted:
            raise VirtualBlockDeviceError("block device is not inserted")
        if self.image_path is None:
            raise VirtualBlockDeviceError("block device has no attached image")
        if self.fail_next_reads > 0:
            self.fail_next_reads -= 1
            raise VirtualBlockDeviceError("injected read failure")

        size = block_count * self.block_size
        with self.image_path.open("rb") as file:
            file.seek(first_block * self.block_size)
            data = file.read(size)

        if len(data) != size:
            raise VirtualBlockDeviceError("short block read")
        return data

    def write_blocks(self, first_block: int, data: bytes) -> None:
        if not self.inserted:
            raise VirtualBlockDeviceError("block device is not inserted")
        if self.image_path is None:
            raise VirtualBlockDeviceError("block device has no attached image")
        if not self.writable:
            raise VirtualBlockDeviceWriteProtected("block device is read-only")
        if self.fail_next_writes > 0:
            self.fail_next_writes -= 1
            raise VirtualBlockDeviceError("injected write failure")
        if len(data) % self.block_size != 0:
            raise VirtualBlockDeviceError("unaligned block write")

        with self.image_path.open("r+b") as file:
            file.seek(first_block * self.block_size)
            file.write(data)
        if self._directory_mirror is not None:
            self._directory_mirror.mark_written()


@dataclass
class VirtualI2CBus:
    present_addresses: Set[int] = field(default_factory=set)
    writes: list[tuple[int, bytes]] = field(default_factory=list)

    def attach(self, address: int) -> None:
        self.present_addresses.add(address & 0x7F)

    def detach(self, address: int) -> None:
        self.present_addresses.discard(address & 0x7F)

    def probe(self, address: int) -> bool:
        return (address & 0x7F) in self.present_addresses

    def attached_addresses(self) -> tuple[int, ...]:
        return tuple(sorted(self.present_addresses))

    def write(self, address: int, data: bytes) -> bool:
        if not self.probe(address):
            return False
        self.writes.append((address & 0x7F, bytes(data)))
        return True


@dataclass
class VirtualBoard:
    clock: VirtualClock = field(default_factory=VirtualClock)
    blocks: Dict[int, VirtualBlockDevice] = field(default_factory=lambda: {0: VirtualBlockDevice()})
    i2c: Dict[int, VirtualI2CBus] = field(default_factory=lambda: {0: VirtualI2CBus()})
    display_size: tuple[int, int] = DEFAULT_DISPLAY_SIZE
    audio: VirtualAudioSink = field(default_factory=VirtualAudioSink)
    logs: list[tuple[int, int, str]] = field(default_factory=list)

    @classmethod
    def dev_pico(
            cls,
            *,
            sd_image: str | Path | None = None,
            sd_directory: str | Path | None = None,
            sd_inserted: bool = False,
            sd_capacity: str = "16G",
            sd_filesystem: str = "fat32",
            sd_writable: bool = False,
            oled: bool = False,
            display_size: str | Iterable[int] | None = None,
            audio_mode: str | int | VirtualAudioMode | None = VirtualAudioMode.PWM,
            audio_output: bool | str = False,
            audio_wav_path: str | Path | None = None,
    ) -> "VirtualBoard":
        board = cls(
            display_size=normalize_display_size(display_size),
            audio=VirtualAudioSink(
                mode=normalize_audio_mode(audio_mode),
                output_enabled=audio_output,
                output_wav_path=audio_wav_path,
            ),
        )

        if sd_image is not None and sd_directory is not None:
            raise ValueError("sd_image and sd_directory are mutually exclusive")

        if sd_image is not None:
            board.blocks[0].attach_image(sd_image, inserted=sd_inserted, writable=sd_writable)
        elif sd_directory is not None:
            board.blocks[0].attach_directory(
                sd_directory, inserted=sd_inserted,
                capacity=sd_capacity, filesystem=sd_filesystem,
            )
        else:
            board.blocks[0].inserted = sd_inserted
        if oled:
            board.i2c[0].attach(0x3C)
        return board

    def block(self, device_id: int) -> VirtualBlockDevice:
        return self.blocks.setdefault(device_id, VirtualBlockDevice())

    def i2c_bus(self, bus: int) -> VirtualI2CBus:
        return self.i2c.setdefault(bus, VirtualI2CBus())

    def close(self) -> None:
        self.audio.stop()
        for block in self.blocks.values():
            block.close()

    def log(self, level: int, message: str) -> None:
        self.logs.append((self.clock.now_us, level, message))
