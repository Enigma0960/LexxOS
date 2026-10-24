from __future__ import annotations

import argparse
import os
import select
import sys
import termios
import tty

from pathlib import Path
from typing import Iterator
from contextlib import contextmanager

from .board import VirtualBoard, normalize_display_size
from .sim import InputModifier, KeyCode, LexxOsSim
from .fat32_export import export_fat32_snapshot


@contextmanager
def _raw_terminal(fd: int) -> Iterator[None]:
    previous = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        yield
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, previous)


def _decode_caret_escape_sequences(data: str) -> str:
    """Decode escape sequences printed by some non-TTY IDE consoles.

    A real terminal sends arrow-left as ESC [ D. Some IDE consoles expose the
    same key press to stdin as the visible text "^[[D" instead. This fallback
    keeps line-mode runs from passing those characters to LexxOS literally.
    It cannot provide true live cursor editing because the IDE still buffers
    input until Enter.
    """
    replacements = {
        "^[[A": "\x1b[A",
        "^[[B": "\x1b[B",
        "^[[C": "\x1b[C",
        "^[[D": "\x1b[D",
        "^[[H": "\x1b[H",
        "^[[F": "\x1b[F",
        "^[OA": "\x1bOA",
        "^[OB": "\x1bOB",
        "^[OC": "\x1bOC",
        "^[OD": "\x1bOD",
        "^[[3~": "\x1b[3~",
    }
    for visible, raw in replacements.items():
        data = data.replace(visible, raw)
    return data


def _normalize_newlines_for_raw_terminal(data: str) -> str:
    """Convert bare LF to CRLF for terminals running with output processing disabled.
    ``tty.setraw`` disables the terminal driver's ONLCR conversion. LexxOS
    mostly emits ``\n`` line endings, which are fine in normal terminals but
    move only down, not back to column zero, in raw mode. Keep explicit ``\r``
    sequences intact because the shell line editor uses them to redraw the
    current input line.
    """
    normalized: list[str] = []
    previous = ""
    for character in data:
        if character == "\n" and previous != "\r":
            normalized.append("\r")
        normalized.append(character)
        previous = character
    return "".join(normalized)


def _write_output(output: str, *, raw_terminal: bool = False) -> None:
    if not output:
        return
    if raw_terminal:
        output = _normalize_newlines_for_raw_terminal(output)
    sys.stdout.write(output)
    sys.stdout.flush()


def _pump_output(sim: LexxOsSim, *, raw_terminal: bool = False) -> None:
    _write_output(sim.read_all(), raw_terminal=raw_terminal)


class _NoDisplayWindow:
    def attach_sim(self, _sim: LexxOsSim) -> None:
        pass

    def refresh(self, _sim: LexxOsSim) -> None:
        pass

    def close(self) -> None:
        pass


class _QtDisplayWindow:
    def __init__(self):
        from PySide6.QtCore import QEvent, Qt
        from PySide6.QtGui import QImage, QPixmap
        from PySide6.QtWidgets import QApplication, QHBoxLayout, QLabel, QPushButton, QVBoxLayout, QWidget

        owner = self

        class DisplayWidget(QWidget):
            def event(self, event):  # noqa: N802 - Qt override name.
                if event.type() == QEvent.Type.KeyPress and owner._handle_key_event(event, True):
                    return True
                if event.type() == QEvent.Type.KeyRelease and owner._handle_key_event(event, False):
                    return True
                return super().event(event)

        self._sim: LexxOsSim | None = None
        self._qt = Qt
        self._qimage = QImage
        self._qpixmap = QPixmap
        self._app = QApplication.instance() or QApplication(sys.argv[:1])
        self._window = DisplayWidget()
        self._window.setWindowTitle("LexxOS emu_display")
        self._window.setFocusPolicy(Qt.FocusPolicy.StrongFocus)
        self._label = QLabel("waiting for the first frame", self._window)
        self._label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._label.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        self._label.setStyleSheet("background: #202020; color: #b0b0b0; border: 1px solid #606060;")
        self._frame_label = QLabel("frame: -", self._window)
        layout = QVBoxLayout(self._window)
        layout.addWidget(self._label)
        layout.addWidget(self._frame_label)
        button_layout = QHBoxLayout()
        self._alt_tab_button = QPushButton("Alt+Tab", self._window)
        self._alt_tab_button.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        self._alt_tab_button.clicked.connect(self.send_alt_tab)
        button_layout.addWidget(self._alt_tab_button)
        self._ctrl_c_button = QPushButton("Ctrl+C", self._window)
        self._ctrl_c_button.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        self._ctrl_c_button.clicked.connect(self.send_ctrl_c)
        button_layout.addWidget(self._ctrl_c_button)
        layout.addLayout(button_layout)
        self._last_frame = -1
        self._visible = False

    def attach_sim(self, sim: LexxOsSim) -> None:
        self._sim = sim

    @staticmethod
    def _scaled_size(width: int, height: int) -> tuple[int, int]:
        max_width = 960
        max_height = 720
        scale = min(max_width / width, max_height / height, 1.0)
        return max(1, int(width * scale)), max(1, int(height * scale))

    def refresh(self, sim: LexxOsSim) -> None:
        info = sim.display_info()
        if int(info.frame_counter) != self._last_frame:
            rgba = sim.display_rgba8888()
            image = self._qimage(rgba, info.width, info.height, info.width * 4, self._qimage.Format.Format_RGBA8888)
            pixmap = self._qpixmap.fromImage(image.copy())
            width, height = self._scaled_size(info.width, info.height)
            self._label.setFixedSize(width, height)
            if (width, height) != (info.width, info.height):
                pixmap = pixmap.scaled(
                    width,
                    height,
                    self._qt.AspectRatioMode.KeepAspectRatio,
                    self._qt.TransformationMode.FastTransformation,
                )
            self._label.setPixmap(pixmap)
            self._frame_label.setText(f"frame: {info.frame_counter} ({info.width}x{info.height})")
            self._last_frame = int(info.frame_counter)

            if not self._visible and info.frame_counter > 0:
                self._window.show()
                self._window.activateWindow()
                self._window.setFocus()
                self._visible = True

        self._app.processEvents()

    def close(self) -> None:
        self._sim = None
        self._window.close()
        self._app.processEvents()

    def send_alt_tab(self) -> None:
        if self._sim is not None:
            self._sim.tap_key_event(KeyCode.TAB, modifiers=InputModifier.ALT)
        self._window.setFocus()

    def send_ctrl_c(self) -> None:
        if self._sim is not None:
            self._sim.tap_key_event(KeyCode.C, character=3, modifiers=InputModifier.CTRL)
        self._window.setFocus()

    def _handle_key_event(self, event, pressed: bool) -> bool:
        if self._sim is None or event.isAutoRepeat():
            return False

        key = self._qt_key_to_lexxos(event.key())
        if key is None:
            return False

        self._sim.push_key_event(
            key,
            pressed,
            character=self._event_character(event) if pressed else 0,
            modifiers=self._event_modifiers(event),
        )
        return True

    def _event_modifiers(self, event) -> int:
        modifiers = 0
        qt_modifiers = event.modifiers()
        if qt_modifiers & self._qt.KeyboardModifier.AltModifier:
            modifiers |= int(InputModifier.ALT)
        if qt_modifiers & self._qt.KeyboardModifier.ControlModifier:
            modifiers |= int(InputModifier.CTRL)
        if qt_modifiers & self._qt.KeyboardModifier.ShiftModifier:
            modifiers |= int(InputModifier.SHIFT_LEFT)
        return modifiers

    @staticmethod
    def _event_character(event) -> int:
        text = event.text()
        if not text:
            return 0
        value = ord(text[0])
        return value if 0 <= value <= 0xFF else 0

    def _qt_key_to_lexxos(self, key: int) -> KeyCode | None:
        qt = self._qt.Key
        mapping = {
            int(qt.Key_Up): KeyCode.UP,
            int(qt.Key_Down): KeyCode.DOWN,
            int(qt.Key_Left): KeyCode.LEFT,
            int(qt.Key_Right): KeyCode.RIGHT,
            int(qt.Key_Return): KeyCode.ENTER,
            int(qt.Key_Enter): KeyCode.ENTER,
            int(qt.Key_Escape): KeyCode.ESCAPE,
            int(qt.Key_Backspace): KeyCode.BACKSPACE,
            int(qt.Key_Space): KeyCode.SPACE,
            int(qt.Key_Tab): KeyCode.TAB,
            int(qt.Key_Backtab): KeyCode.TAB,
            int(qt.Key_Delete): KeyCode.DELETE,
            int(qt.Key_Insert): KeyCode.INSERT,
            int(qt.Key_Home): KeyCode.HOME,
            int(qt.Key_End): KeyCode.END,
            int(qt.Key_Minus): KeyCode.MINUS,
            int(qt.Key_Equal): KeyCode.EQUALS,
            int(qt.Key_BracketLeft): KeyCode.LEFT_BRACKET,
            int(qt.Key_BracketRight): KeyCode.RIGHT_BRACKET,
            int(qt.Key_Backslash): KeyCode.BACKSLASH,
            int(qt.Key_Semicolon): KeyCode.SEMICOLON,
            int(qt.Key_Apostrophe): KeyCode.APOSTROPHE,
            int(qt.Key_Comma): KeyCode.COMMA,
            int(qt.Key_Period): KeyCode.PERIOD,
            int(qt.Key_Slash): KeyCode.SLASH,
            int(qt.Key_QuoteLeft): KeyCode.GRAVE,
            int(qt.Key_F1): KeyCode.F1,
            int(qt.Key_F2): KeyCode.F2,
            int(qt.Key_F3): KeyCode.F3,
            int(qt.Key_F4): KeyCode.F4,
            int(qt.Key_F5): KeyCode.F5,
            int(qt.Key_F6): KeyCode.F6,
            int(qt.Key_F7): KeyCode.F7,
            int(qt.Key_F8): KeyCode.F8,
            int(qt.Key_F9): KeyCode.F9,
            int(qt.Key_F10): KeyCode.F10,
        }
        if key in mapping:
            return mapping[key]
        if int(qt.Key_A) <= key <= int(qt.Key_Z):
            return KeyCode[chr(ord("A") + key - int(qt.Key_A))]
        if int(qt.Key_0) <= key <= int(qt.Key_9):
            return KeyCode[f"NUM{key - int(qt.Key_0)}"]
        return None


def _create_display_window(mode: str):
    if mode == "off":
        return _NoDisplayWindow()
    try:
        return _QtDisplayWindow()
    except ImportError as exc:
        message = "warning: PySide6 is unavailable; emu_display popup is disabled.\n"
        if mode == "on":
            raise RuntimeError(message.strip()) from exc
        sys.stderr.write(message)
        sys.stderr.flush()
        return _NoDisplayWindow()


def _host_gui_session_available() -> bool:
    if sys.platform.startswith("linux"):
        return bool(os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY"))
    return True


def _effective_display_window_mode(requested: str) -> str:
    if requested != "auto":
        return requested
    if not sys.stdin.isatty() or not _host_gui_session_available():
        return "off"
    return "auto"


def _run_interactive_tty(sim: LexxOsSim, display_window) -> None:
    stdin_fd = sys.stdin.fileno()
    with _raw_terminal(stdin_fd):
        while sim.is_running():
            ready, _, _ = select.select([stdin_fd], [], [], 0.01)
            if ready:
                data = os.read(stdin_fd, 1024)
                if not data:
                    break
                if b"\x04" in data:
                    sim.request_shutdown()
                    break
                sim.write(data)
            sim.run_ms(10)
            _pump_output(sim, raw_terminal=True)
            display_window.refresh(sim)


def _run_line_mode(sim: LexxOsSim, display_window) -> None:
    sys.stderr.write(
        "warning: stdin is not a TTY; falling back to buffered line input.\n"
        "warning: run from a real terminal to use live cursor movement.\n"
    )
    sys.stderr.flush()
    for line in sys.stdin:
        sim.write(_decode_caret_escape_sequences(line))
        _write_output(sim.run_until_idle())
        display_window.refresh(sim)
        if not sim.is_running():
            break


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Run LexxOS inside the Python simulator")
    parser.add_argument("--lib", required=True, help="Path to liblexxos_sim.dylib/.so")
    storage = parser.add_mutually_exclusive_group()
    storage.add_argument("--sd-image", help="Optional raw SD/FAT image exposed as /dev/sd0")
    storage.add_argument("--sd-dir", help="Optional host directory imported as /dev/sd0 FAT contents")
    parser.add_argument("--sd-capacity", choices=("256M", "512M", "16G", "32G"), default="16G")
    parser.add_argument("--sd-fs", choices=("fat32", "fat16"), default="fat32")
    parser.add_argument("--sd-writable", action="store_true", help="Enable persistent image writes")
    parser.add_argument("--sd-export-to", help="Export stopped FAT32 session into a NEW host directory")
    parser.add_argument("--oled", action="store_true", help="Expose a simulated SSD1306 device at I2C address 0x3C")
    parser.add_argument("--display-size", default="320x320", help="Initial emu_display size, for example 320x240")
    parser.add_argument(
        "--display-window",
        choices=("auto", "on", "off"),
        default="auto",
        help="Open a popup window for emu_display frames when PySide6 is available",
    )
    parser.add_argument("--audio-mode", choices=("off", "pwm", "adc"), default="pwm",
                        help="Expose emulated audio output mode")
    parser.add_argument(
        "--audio-output",
        choices=("auto", "on", "off", "wav"),
        default="auto",
        help="Host audio output: sounddevice stream in auto/on, disabled in off, or write one WAV file in wav",
    )
    parser.add_argument(
        "--audio-wav-path",
        default="lexxos-audio.wav",
        help="Output path used with --audio-output wav",
    )
    args = parser.parse_args(argv)

    try:
        display_size = normalize_display_size(args.display_size)
    except ValueError as exc:
        parser.error(str(exc))

    has_sd = bool(args.sd_image or args.sd_dir)
    try:
        board = VirtualBoard.dev_pico(
            sd_image=args.sd_image,
            sd_directory=args.sd_dir,
            sd_inserted=has_sd,
            sd_capacity=args.sd_capacity,
            sd_filesystem=args.sd_fs,
            sd_writable=args.sd_writable,
            oled=args.oled,
            display_size=display_size,
            audio_mode=args.audio_mode,
            audio_output=args.audio_output,
            audio_wav_path=Path(args.audio_wav_path).expanduser() if args.audio_output == "wav" else None,
        )
    except (RuntimeError, OSError, ValueError) as exc:
        parser.exit(2, f"error: {exc}\n")
    try:
        display_window = _create_display_window(_effective_display_window_mode(args.display_window))
    except RuntimeError as exc:
        parser.exit(2, f"error: {exc}\n")
    with LexxOsSim(args.lib, board) as sim:
        display_window.attach_sim(sim)
        sim.set_sd_inserted(has_sd)
        sim.boot()
        sys.stdout.write(sim.run_until_output_contains("lexx:/>", timeout=5.0))
        sys.stdout.flush()

        try:
            if sys.stdin.isatty():
                _run_interactive_tty(sim, display_window)
            else:
                _run_line_mode(sim, display_window)
        except KeyboardInterrupt:
            sim.request_shutdown()
        finally:
            display_window.close()
            if args.sd_export_to:
                if sim.is_running():
                    print("SD export skipped: stop LexxOS first", file=sys.stderr)
                else:
                    image = board.block(0).image_path
                    if image is not None:
                        export_fat32_snapshot(image, Path(args.sd_export_to).expanduser())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
