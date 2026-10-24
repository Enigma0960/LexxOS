from __future__ import annotations

import ctypes
import time
from pathlib import Path

from .board import VirtualBoard
from .sim_abi import SimBoardApiBridge, configure_sim_library
from .sim_types import DisplayInfo, InputModifier, KEY_ALIASES, KeyCode, LexxOsSimError


class LexxOsSim:
    def __init__(self, library_path: str | Path, board: VirtualBoard | None = None):
        self.library_path = Path(library_path)
        self.board = board or VirtualBoard.dev_pico()
        self._lib = ctypes.CDLL(str(self.library_path))
        configure_sim_library(self._lib)
        self._api_bridge = SimBoardApiBridge(self.board)
        self._handle = self._lib.lexxos_sim_create(ctypes.byref(self._api_bridge.api))
        if not self._handle:
            raise LexxOsSimError("failed to create LexxOS simulator")

    def close(self) -> None:
        if self._handle:
            self._lib.lexxos_sim_request_shutdown(self._handle)
            self._lib.lexxos_sim_destroy(self._handle)
            self._handle = None
        self.board.close()

    def __enter__(self) -> "LexxOsSim":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()

    def boot(self) -> None:
        self._check(self._lib.lexxos_sim_boot(self._handle), "boot failed")

    def request_shutdown(self) -> None:
        self._check(self._lib.lexxos_sim_request_shutdown(self._handle), "shutdown request failed")

    def is_running(self) -> bool:
        return bool(self._lib.lexxos_sim_is_running(self._handle))

    def run_ms(self, milliseconds: int) -> None:
        self.board.clock.advance_ms(milliseconds)
        result = self._lib.lexxos_sim_run_ms(self._handle, int(milliseconds))
        if result not in (0, -4):
            self._check(result, "run failed")

    def write(self, data: bytes | str) -> None:
        if isinstance(data, str):
            data = data.encode("utf-8")
        buffer = (ctypes.c_uint8 * len(data)).from_buffer_copy(data)
        self._check(
            self._lib.lexxos_sim_uart_push_rx(self._handle, 0, buffer, len(data)),
            "UART RX push failed",
        )

    def write_line(self, line: str) -> None:
        self.write(line + "\n")

    def read(self, max_size: int = 65536) -> str:
        buffer = (ctypes.c_uint8 * max_size)()
        count = self._lib.lexxos_sim_uart_pop_tx(self._handle, 0, buffer, max_size)
        data = bytes(buffer[:count])
        return data.decode("utf-8", errors="replace")

    def read_all(self) -> str:
        chunks: list[str] = []
        while self.tx_size() > 0:
            chunks.append(self.read())
        return "".join(chunks)

    def tx_size(self) -> int:
        return int(self._lib.lexxos_sim_uart_tx_size(self._handle, 0))

    def set_sd_inserted(self, inserted: bool) -> None:
        self.board.block(0).inserted = inserted
        # GP22 on the current dev_pico board is active-low card-detect.
        self.gpio_set_input(22, not inserted)

    def gpio_set_input(self, pin: int, value: bool) -> None:
        self._check(
            self._lib.lexxos_sim_gpio_set_input(self._handle, int(pin), 1 if value else 0),
            "GPIO input update failed",
        )

    def gpio_get_output(self, pin: int) -> bool:
        value = ctypes.c_int(0)
        self._check(
            self._lib.lexxos_sim_gpio_get_output(self._handle, int(pin), ctypes.byref(value)),
            "GPIO output read failed",
        )
        return bool(value.value)

    def set_key(self, key: KeyCode | str | int, pressed: bool) -> None:
        resolved = self._resolve_key_code(key)
        self._check(
            self._lib.lexxos_sim_input_set_key(
                self._handle,
                ctypes.c_uint16(int(resolved)),
                1 if pressed else 0,
            ),
            "input key update failed",
        )

    def tap_key(self, key: KeyCode | str | int) -> None:
        self.set_key(key, True)
        self.set_key(key, False)

    def push_key_event(
            self,
            key: KeyCode | str | int,
            pressed: bool,
            *,
            character: str | int = 0,
            modifiers: int | InputModifier = 0,
    ) -> None:
        resolved = self._resolve_key_code(key)
        character_code = self._resolve_character(character)
        self._check(
            self._lib.lexxos_sim_input_push_event(
                self._handle,
                ctypes.c_uint16(int(resolved)),
                1 if pressed else 0,
                ctypes.c_uint8(character_code),
                ctypes.c_uint8(int(modifiers) & 0xFF),
            ),
            "input key event push failed",
        )

    def tap_key_event(
            self,
            key: KeyCode | str | int,
            *,
            character: str | int = 0,
            modifiers: int | InputModifier = 0,
    ) -> None:
        self.push_key_event(key, True, character=character, modifiers=modifiers)
        self.push_key_event(key, False, modifiers=modifiers)

    def display_info(self) -> DisplayInfo:
        width = ctypes.c_int(0)
        height = ctypes.c_int(0)
        pixel_format = ctypes.c_int(0)
        frame_counter = ctypes.c_uint32(0)
        self._check(
            self._lib.lexxos_sim_display_info(
                self._handle,
                ctypes.byref(width),
                ctypes.byref(height),
                ctypes.byref(pixel_format),
                ctypes.byref(frame_counter),
            ),
            "display info read failed",
        )
        return DisplayInfo(width.value, height.value, pixel_format.value, frame_counter.value)

    def display_rgb565(self) -> bytes:
        info = self.display_info()
        pixel_count = info.width * info.height
        buffer = (ctypes.c_uint16 * pixel_count)()
        copied = self._lib.lexxos_sim_display_copy_rgb565(self._handle, buffer, pixel_count)
        if copied != pixel_count:
            raise LexxOsSimError(f"display snapshot read failed: copied {copied} of {pixel_count} pixels")
        return bytes(buffer)

    def display_rgba8888(self) -> bytes:
        rgb565 = self.display_rgb565()
        result = bytearray()
        for index in range(0, len(rgb565), 2):
            value = rgb565[index] | (rgb565[index + 1] << 8)
            red5 = (value >> 11) & 0x1F
            green6 = (value >> 5) & 0x3F
            blue5 = value & 0x1F
            red = (red5 << 3) | (red5 >> 2)
            green = (green6 << 2) | (green6 >> 4)
            blue = (blue5 << 3) | (blue5 >> 2)
            result.extend((red, green, blue, 255))
        return bytes(result)

    def run_until_output_contains(self, text: str, *, timeout: float = 5.0, poll_ms: int = 10) -> str:
        deadline = time.monotonic() + timeout
        output = ""
        while time.monotonic() < deadline:
            self.run_ms(poll_ms)
            output += self.read_all()
            if text in output:
                return output
        raise TimeoutError(f"output did not contain {text!r}; got: {output!r}")

    def run_until_idle(self, *, rounds: int = 5, poll_ms: int = 10) -> str:
        output = ""
        idle_rounds = 0
        while idle_rounds < rounds:
            self.run_ms(poll_ms)
            chunk = self.read_all()
            if chunk:
                output += chunk
                idle_rounds = 0
            else:
                idle_rounds += 1
        return output

    @staticmethod
    def _resolve_key_code(key: KeyCode | str | int) -> KeyCode:
        if isinstance(key, KeyCode):
            return key
        if isinstance(key, str):
            normalized = key.strip().lower().replace("-", "_")
            if normalized.startswith("num") and len(normalized) == 4 and normalized[-1].isdigit():
                normalized = normalized[-1]
            try:
                return KEY_ALIASES[normalized]
            except KeyError as exc:
                raise ValueError(f"unknown simulated key: {key!r}") from exc
        return KeyCode(int(key))

    @staticmethod
    def _resolve_character(character: str | int) -> int:
        if isinstance(character, str):
            if not character:
                return 0
            return ord(character[0]) & 0xFF
        return int(character) & 0xFF

    @staticmethod
    def _check(result: int, message: str) -> None:
        if result != 0:
            raise LexxOsSimError(f"{message}: {result}")
