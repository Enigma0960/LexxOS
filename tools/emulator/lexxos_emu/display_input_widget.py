"""Keyboard focus surface for the simulated display (not the UART terminal).

When the LCD has focus, physical key transitions are forwarded through the
simulator's input-device event queue, just like keys on PicoCalc hardware.
"""

from __future__ import annotations

from PySide6.QtCore import QEvent, Qt, Signal
from PySide6.QtWidgets import QLabel

from .sim_types import InputModifier, KeyCode

_SPECIAL_KEYS = {
    Qt.Key.Key_Up: KeyCode.UP,
    Qt.Key.Key_Down: KeyCode.DOWN,
    Qt.Key.Key_Left: KeyCode.LEFT,
    Qt.Key.Key_Right: KeyCode.RIGHT,
    Qt.Key.Key_Return: KeyCode.ENTER,
    Qt.Key.Key_Enter: KeyCode.ENTER,
    Qt.Key.Key_Escape: KeyCode.ESCAPE,
    Qt.Key.Key_Backspace: KeyCode.BACKSPACE,
    Qt.Key.Key_Space: KeyCode.SPACE,
    Qt.Key.Key_Tab: KeyCode.TAB,
    Qt.Key.Key_Backtab: KeyCode.TAB,
    Qt.Key.Key_QuoteLeft: KeyCode.GRAVE,
    Qt.Key.Key_AsciiTilde: KeyCode.GRAVE,
    Qt.Key.Key_Minus: KeyCode.MINUS,
    Qt.Key.Key_Equal: KeyCode.EQUALS,
    Qt.Key.Key_BracketLeft: KeyCode.LEFT_BRACKET,
    Qt.Key.Key_BracketRight: KeyCode.RIGHT_BRACKET,
    Qt.Key.Key_Backslash: KeyCode.BACKSLASH,
    Qt.Key.Key_Semicolon: KeyCode.SEMICOLON,
    Qt.Key.Key_Apostrophe: KeyCode.APOSTROPHE,
    Qt.Key.Key_Comma: KeyCode.COMMA,
    Qt.Key.Key_Period: KeyCode.PERIOD,
    Qt.Key.Key_Slash: KeyCode.SLASH,
    Qt.Key.Key_Home: KeyCode.HOME,
    Qt.Key.Key_End: KeyCode.END,
    Qt.Key.Key_Delete: KeyCode.DELETE,
    Qt.Key.Key_Insert: KeyCode.INSERT,
}


def _mapped_key(qt_key: int, text: str) -> KeyCode | None:
    if key := _SPECIAL_KEYS.get(qt_key):
        return key
    if Qt.Key.Key_A <= qt_key <= Qt.Key.Key_Z:
        return KeyCode[chr(ord("A") + int(qt_key) - int(Qt.Key.Key_A))]
    if Qt.Key.Key_0 <= qt_key <= Qt.Key.Key_9:
        return KeyCode["NUM" + str(int(qt_key) - int(Qt.Key.Key_0))]
    if Qt.Key.Key_F1 <= qt_key <= Qt.Key.Key_F10:
        return KeyCode["F" + str(int(qt_key) - int(Qt.Key.Key_F1) + 1)]
    # Unsupported physical keys with a single Latin-1 character can still
    # provide text to a guest editor without inventing a hardware key code.
    if len(text) == 1 and ord(text) <= 255:
        return KeyCode.UNKNOWN
    return None


def _character(qt_key: int, text: str, modifiers: Qt.KeyboardModifier) -> int:
    fixed = {
        Qt.Key.Key_Return: 13,
        Qt.Key.Key_Enter: 13,
        Qt.Key.Key_Tab: 9,
        Qt.Key.Key_Backtab: 9,
        Qt.Key.Key_Escape: 27,
        Qt.Key.Key_Backspace: 8,
        Qt.Key.Key_Space: 32,
    }
    if qt_key in fixed:
        return fixed[qt_key]
    if (modifiers & Qt.KeyboardModifier.ControlModifier) and Qt.Key.Key_A <= qt_key <= Qt.Key.Key_Z:
        return int(qt_key) - int(Qt.Key.Key_A) + 1
    if len(text) == 1 and ord(text) <= 255:
        return ord(text)
    # The present SimInputEvent ABI contains an 8-bit character. Do not
    # truncate Unicode codepoints; a separate UTF-8/IME API is needed.
    return 0


def _modifiers(qt_modifiers: Qt.KeyboardModifier) -> int:
    result = 0
    if qt_modifiers & Qt.KeyboardModifier.AltModifier:
        result |= int(InputModifier.ALT)
    if qt_modifiers & Qt.KeyboardModifier.ShiftModifier:
        result |= int(InputModifier.SHIFT_LEFT)
    if qt_modifiers & Qt.KeyboardModifier.ControlModifier:
        result |= int(InputModifier.CTRL)
    return result


class DisplayInputWidget(QLabel):
    """The displayed framebuffer accepts keys only while it has Qt focus.

    The parent GUI connects `input_event` to LexxOsSim.push_key_event().
    Releasing focus also releases held keys, avoiding stuck game controls.
    """

    input_event = Signal(int, bool, int, int)  # KeyCode, down, char, modifiers

    def __init__(self, parent=None):
        super().__init__("display is not running", parent)
        self._pressed: dict[int, KeyCode] = {}
        self.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.setFixedSize(320, 320)
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)
        self.setToolTip("Click to control the emulated display; click console for UART input")
        self._update_border()

    def _update_border(self) -> None:
        border = "#21bed4" if self.hasFocus() else "#606060"
        self.setStyleSheet(f"background: #202020; color: #b0b0b0; border: 1px solid {border};")

    def focusNextPrevChild(self, next: bool) -> bool:  # noqa: N802
        # Keep Tab/Shift+Tab inside the emulated full-screen application.
        return False

    def mousePressEvent(self, event):  # noqa: N802
        self.setFocus(Qt.FocusReason.MouseFocusReason)
        event.accept()

    def focusInEvent(self, event):  # noqa: N802
        super().focusInEvent(event)
        self._update_border()

    def focusOutEvent(self, event):  # noqa: N802
        self.release_pressed_keys()
        super().focusOutEvent(event)
        self._update_border()

    def release_pressed_keys(self) -> None:
        for key in tuple(self._pressed.values()):
            self.input_event.emit(int(key), False, 0, 0)
        self._pressed.clear()

    def event(self, event):
        if event.type() == QEvent.Type.ShortcutOverride and self.hasFocus():
            # A focused display owns its keys, including keys used by toolbar
            # shortcuts such as Ctrl+B and Ctrl+Shift+C.
            if _mapped_key(event.key(), event.text()) is not None:
                event.accept()
                return True
        return super().event(event)

    def keyPressEvent(self, event):  # noqa: N802
        if event.isAutoRepeat():
            event.accept()  # Preserve hardware held-key state.
            return
        qt_key = int(event.key())
        physical_key = int(Qt.Key.Key_Tab) if qt_key == Qt.Key.Key_Backtab else qt_key
        key = _mapped_key(qt_key, event.text())
        if key is not None and physical_key not in self._pressed:
            self._pressed[physical_key] = key
            self.input_event.emit(int(key), True,
                                  _character(qt_key, event.text(), event.modifiers()),
                                  _modifiers(event.modifiers()))
        event.accept()

    def keyReleaseEvent(self, event):  # noqa: N802
        if not event.isAutoRepeat():
            qt_key = int(event.key())
            physical_key = int(Qt.Key.Key_Tab) if qt_key == Qt.Key.Key_Backtab else qt_key
            key = self._pressed.pop(physical_key, None)
            if key is not None:
                self.input_event.emit(int(key), False, 0, _modifiers(event.modifiers()))
        event.accept()
