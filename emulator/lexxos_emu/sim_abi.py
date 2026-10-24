from __future__ import annotations

import ctypes

from .board import VirtualBlockDeviceError, VirtualBlockDeviceWriteProtected, VirtualBoard

# Matches LexxOsSimResult in native/lexxos_sim_api.h.
LEXXOS_SIM_WRITE_PROTECTED = -5
LEXXOS_SIM_BLOCK_INSERTED = 1
LEXXOS_SIM_BLOCK_READ_ONLY = 2

NOW_US_CB = ctypes.CFUNCTYPE(ctypes.c_uint64, ctypes.c_void_p)
BLOCK_IS_INSERTED_CB = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_uint8)
BLOCK_COUNT_CB = ctypes.CFUNCTYPE(ctypes.c_uint64, ctypes.c_void_p, ctypes.c_uint8)
BLOCK_READ_CB = ctypes.CFUNCTYPE(
    ctypes.c_int,
    ctypes.c_void_p,
    ctypes.c_uint8,
    ctypes.c_uint64,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_size_t,
)
BLOCK_WRITE_CB = ctypes.CFUNCTYPE(
    ctypes.c_int,
    ctypes.c_void_p,
    ctypes.c_uint8,
    ctypes.c_uint64,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_size_t,
)
I2C_PROBE_CB = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_uint8, ctypes.c_uint8)
I2C_WRITE_CB = ctypes.CFUNCTYPE(
    ctypes.c_int,
    ctypes.c_void_p,
    ctypes.c_uint8,
    ctypes.c_uint8,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_size_t,
)
LOG_CB = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_int, ctypes.c_char_p)
AUDIO_TONE_CB = ctypes.CFUNCTYPE(
    ctypes.c_int,
    ctypes.c_void_p,
    ctypes.c_uint8,
    ctypes.c_uint8,
    ctypes.c_uint32,
    ctypes.c_uint32,
    ctypes.c_uint8,
)
AUDIO_PCM_CB = ctypes.CFUNCTYPE(
    ctypes.c_int,
    ctypes.c_void_p,
    ctypes.c_uint8,
    ctypes.c_uint8,
    ctypes.POINTER(ctypes.c_int16),
    ctypes.c_size_t,
    ctypes.c_uint32,
)
AUDIO_DRAIN_CB = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_uint8, ctypes.c_uint32)
AUDIO_STOP_CB = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_uint8)
HOST_DIRECTORY_SYNC_CB = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_int)


class LexxOsSimBoardApi(ctypes.Structure):
    _fields_ = [
        ("user", ctypes.c_void_p),
        ("display_width", ctypes.c_uint16),
        ("display_height", ctypes.c_uint16),
        ("audio_mode", ctypes.c_uint8),
        ("reserved", ctypes.c_uint8 * 3),
        ("now_us", NOW_US_CB),
        ("block_is_inserted", BLOCK_IS_INSERTED_CB),
        ("block_count", BLOCK_COUNT_CB),
        ("block_read", BLOCK_READ_CB),
        ("block_write", BLOCK_WRITE_CB),
        ("i2c_probe", I2C_PROBE_CB),
        ("i2c_write", I2C_WRITE_CB),
        ("log", LOG_CB),
        ("audio_tone", AUDIO_TONE_CB),
        ("audio_pcm", AUDIO_PCM_CB),
        ("audio_drain", AUDIO_DRAIN_CB),
        ("audio_stop", AUDIO_STOP_CB),
        ("host_directory_sync", HOST_DIRECTORY_SYNC_CB),
    ]


def configure_sim_library(library: ctypes.CDLL) -> None:
    library.lexxos_sim_create.argtypes = [ctypes.POINTER(LexxOsSimBoardApi)]
    library.lexxos_sim_create.restype = ctypes.c_void_p
    library.lexxos_sim_destroy.argtypes = [ctypes.c_void_p]
    library.lexxos_sim_boot.argtypes = [ctypes.c_void_p]
    library.lexxos_sim_boot.restype = ctypes.c_int
    library.lexxos_sim_request_shutdown.argtypes = [ctypes.c_void_p]
    library.lexxos_sim_request_shutdown.restype = ctypes.c_int
    library.lexxos_sim_is_running.argtypes = [ctypes.c_void_p]
    library.lexxos_sim_is_running.restype = ctypes.c_int
    library.lexxos_sim_run_ms.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
    library.lexxos_sim_run_ms.restype = ctypes.c_int
    library.lexxos_sim_uart_push_rx.argtypes = [
        ctypes.c_void_p,
        ctypes.c_uint8,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
    ]
    library.lexxos_sim_uart_push_rx.restype = ctypes.c_int
    library.lexxos_sim_uart_pop_tx.argtypes = [
        ctypes.c_void_p,
        ctypes.c_uint8,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
    ]
    library.lexxos_sim_uart_pop_tx.restype = ctypes.c_size_t
    library.lexxos_sim_uart_tx_size.argtypes = [ctypes.c_void_p, ctypes.c_uint8]
    library.lexxos_sim_uart_tx_size.restype = ctypes.c_size_t
    library.lexxos_sim_gpio_set_input.argtypes = [ctypes.c_void_p, ctypes.c_uint8, ctypes.c_int]
    library.lexxos_sim_gpio_set_input.restype = ctypes.c_int
    library.lexxos_sim_gpio_get_output.argtypes = [
        ctypes.c_void_p,
        ctypes.c_uint8,
        ctypes.POINTER(ctypes.c_int),
    ]
    library.lexxos_sim_gpio_get_output.restype = ctypes.c_int
    library.lexxos_sim_input_set_key.argtypes = [ctypes.c_void_p, ctypes.c_uint16, ctypes.c_int]
    library.lexxos_sim_input_set_key.restype = ctypes.c_int
    library.lexxos_sim_input_push_event.argtypes = [
        ctypes.c_void_p,
        ctypes.c_uint16,
        ctypes.c_int,
        ctypes.c_uint8,
        ctypes.c_uint8,
    ]
    library.lexxos_sim_input_push_event.restype = ctypes.c_int
    library.lexxos_sim_display_info.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_uint32),
    ]
    library.lexxos_sim_display_info.restype = ctypes.c_int
    library.lexxos_sim_display_copy_rgb565.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_uint16),
        ctypes.c_size_t,
    ]
    library.lexxos_sim_display_copy_rgb565.restype = ctypes.c_size_t


class SimBoardApiBridge:
    def __init__(self, board: VirtualBoard):
        self.board = board
        self._callbacks = [
            NOW_US_CB(self._now_us),
            BLOCK_IS_INSERTED_CB(self._block_is_inserted),
            BLOCK_COUNT_CB(self._block_count),
            BLOCK_READ_CB(self._block_read),
            BLOCK_WRITE_CB(self._block_write),
            I2C_PROBE_CB(self._i2c_probe),
            I2C_WRITE_CB(self._i2c_write),
            LOG_CB(self._log),
            AUDIO_TONE_CB(self._audio_tone),
            AUDIO_PCM_CB(self._audio_pcm),
            AUDIO_DRAIN_CB(self._audio_drain),
            AUDIO_STOP_CB(self._audio_stop),
            HOST_DIRECTORY_SYNC_CB(self._host_directory_sync),
        ]
        self.api = LexxOsSimBoardApi(
            None,
            int(self.board.display_size[0]),
            int(self.board.display_size[1]),
            int(self.board.audio.mode),
            (ctypes.c_uint8 * 3)(),
            *self._callbacks,
        )

    def _now_us(self, _user) -> int:
        return self.board.clock.now_us

    def _block_is_inserted(self, _user, device_id: int) -> int:
        block = self.board.block(device_id)
        if not block.inserted:
            return 0
        return LEXXOS_SIM_BLOCK_INSERTED | (0 if block.writable else LEXXOS_SIM_BLOCK_READ_ONLY)

    def _block_count(self, _user, device_id: int) -> int:
        return self.board.block(device_id).block_count

    def _block_read(self, _user, device_id: int, first_block: int, dst, block_count: int) -> int:
        try:
            data = self.board.block(device_id).read_blocks(first_block, block_count)
        except VirtualBlockDeviceError as exc:
            self.board.log(3, str(exc))
            return -1
        ctypes.memmove(dst, data, len(data))
        return 0

    def _block_write(self, _user, device_id: int, first_block: int, src, block_count: int) -> int:
        try:
            size = int(block_count) * self.board.block(device_id).block_size
            data = ctypes.string_at(src, size)
            self.board.block(device_id).write_blocks(first_block, data)
        except VirtualBlockDeviceWriteProtected as exc:
            self.board.log(3, str(exc))
            return LEXXOS_SIM_WRITE_PROTECTED
        except VirtualBlockDeviceError as exc:
            self.board.log(3, str(exc))
            return -1
        return 0

    def _i2c_probe(self, _user, bus: int, address: int) -> int:
        return 0 if self.board.i2c_bus(bus).probe(address) else -1

    def _i2c_write(self, _user, bus: int, address: int, data, size: int) -> int:
        payload = ctypes.string_at(data, size)
        return 0 if self.board.i2c_bus(bus).write(address, payload) else -1

    def _log(self, _user, level: int, message) -> None:
        text = message.decode("utf-8", errors="replace") if message else ""
        self.board.log(level, text)

    def _audio_tone(
            self,
            _user,
            mode: int,
            channel: int,
            frequency_hz: int,
            duration_ms: int,
            volume: int,
    ) -> int:
        return 0 if self.board.audio.play_tone(
            mode=mode,
            channel=channel,
            frequency_hz=frequency_hz,
            duration_ms=duration_ms,
            volume=volume,
        ) else -1

    def _audio_pcm(
            self,
            _user,
            mode: int,
            channel: int,
            frames,
            frame_count: int,
            sample_rate_hz: int,
    ) -> int:
        if frames is None and frame_count:
            return -1
        frame_count = int(frame_count)
        pcm_bytes = ctypes.string_at(frames, frame_count * 4) if frames is not None and frame_count else b""
        peak_left = 0
        peak_right = 0
        for index in range(frame_count):
            left = int(frames[index * 2])
            right = int(frames[index * 2 + 1])
            peak_left = max(peak_left, abs(left))
            peak_right = max(peak_right, abs(right))
        return 0 if self.board.audio.play_pcm_buffer(
            mode=mode,
            channel=channel,
            frame_count=frame_count,
            sample_rate_hz=sample_rate_hz,
            peak_left=peak_left,
            peak_right=peak_right,
            pcm_bytes=pcm_bytes,
        ) else -1

    def _audio_drain(self, _user, _mode: int, timeout_ms: int) -> int:
        return 0 if self.board.audio.drain(timeout_ms=timeout_ms) else -1

    def _audio_stop(self, _user, _mode: int) -> None:
        self.board.audio.stop()

    def _host_directory_sync(self, _user, stage: int) -> int:
        try:
            return self.board.block(0).sync_directory(stage)
        except (OSError, ValueError, RuntimeError) as exc:
            # Exceptions MUST NOT escape a ctypes callback into C++.
            self.board.block(0).sync_directory(2)
            self.board.log(3, f"host directory sync failed: {exc}")
            return -1
