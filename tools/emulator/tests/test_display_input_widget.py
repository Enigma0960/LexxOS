"""Offscreen regression checks for focused LCD keyboard forwarding.

Run with: QT_QPA_PLATFORM=offscreen python -m pytest -q
    tools/emulator/tests/test_display_input_widget.py
"""

import os

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

import pytest

pytest.importorskip("PySide6")

from PySide6.QtCore import QEvent, Qt
from PySide6.QtGui import QFocusEvent, QKeyEvent
from PySide6.QtWidgets import QApplication

from lexxos_emu.display_input_widget import DisplayInputWidget
from lexxos_emu.sim_types import InputModifier, KeyCode


@pytest.fixture
def display():
    app = QApplication.instance() or QApplication([])
    widget = DisplayInputWidget()
    widget.show()
    widget.activateWindow()
    widget.setFocus(Qt.FocusReason.MouseFocusReason)
    app.processEvents()
    yield widget
    widget.release_pressed_keys()
    widget.close()
    app.processEvents()


def _send(widget, kind, key, text="", modifiers=Qt.KeyboardModifier.NoModifier, repeat=False):
    event = QKeyEvent(kind, key, modifiers, text, repeat)
    QApplication.sendEvent(widget, event)
    return event


def test_tab_and_arrows_are_emulated_not_sent_to_qt_focus(display):
    events = []
    display.input_event.connect(lambda *args: events.append(args))
    assert display.hasFocus()
    assert display.focusNextPrevChild(True) is False
    _send(display, QEvent.Type.KeyPress, Qt.Key.Key_Tab, "\t")
    _send(display, QEvent.Type.KeyRelease, Qt.Key.Key_Tab)
    _send(display, QEvent.Type.KeyPress, Qt.Key.Key_Down)
    _send(display, QEvent.Type.KeyRelease, Qt.Key.Key_Down)
    assert events == [
        (int(KeyCode.TAB), True, 9, 0),
        (int(KeyCode.TAB), False, 0, 0),
        (int(KeyCode.DOWN), True, 0, 0),
        (int(KeyCode.DOWN), False, 0, 0),
    ]


def test_text_and_modifiers_are_forwarded(display):
    events = []
    display.input_event.connect(lambda *args: events.append(args))
    _send(display, QEvent.Type.KeyPress, Qt.Key.Key_A, "A", Qt.KeyboardModifier.ShiftModifier)
    _send(display, QEvent.Type.KeyRelease, Qt.Key.Key_A)
    _send(display, QEvent.Type.KeyPress, Qt.Key.Key_C, "", Qt.KeyboardModifier.ControlModifier)
    _send(display, QEvent.Type.KeyRelease, Qt.Key.Key_C)
    assert events[0] == (int(KeyCode.A), True, ord("A"), int(InputModifier.SHIFT_LEFT))
    assert events[1] == (int(KeyCode.A), False, 0, 0)
    assert events[2] == (int(KeyCode.C), True, 3, int(InputModifier.CTRL))
    assert events[3] == (int(KeyCode.C), False, 0, 0)


def test_auto_repeat_does_not_make_a_held_key_bounce(display):
    events = []
    display.input_event.connect(lambda *args: events.append(args))
    _send(display, QEvent.Type.KeyPress, Qt.Key.Key_Right)
    _send(display, QEvent.Type.KeyRelease, Qt.Key.Key_Right, repeat=True)
    _send(display, QEvent.Type.KeyPress, Qt.Key.Key_Right, repeat=True)
    _send(display, QEvent.Type.KeyRelease, Qt.Key.Key_Right)
    assert [(code, pressed) for code, pressed, _, _ in events] == [
        (int(KeyCode.RIGHT), True), (int(KeyCode.RIGHT), False)
    ]


def test_losing_focus_releases_held_keys(display):
    events = []
    display.input_event.connect(lambda *args: events.append(args))
    _send(display, QEvent.Type.KeyPress, Qt.Key.Key_Left)
    QApplication.sendEvent(display, QFocusEvent(QEvent.Type.FocusOut, Qt.FocusReason.OtherFocusReason))
    assert [(code, down) for code, down, _, _ in events] == [
        (int(KeyCode.LEFT), True), (int(KeyCode.LEFT), False)
    ]
    assert display._pressed == {}


def test_focused_screen_overrides_toolbar_shortcuts(display):
    event = _send(display, QEvent.Type.ShortcutOverride,
                  Qt.Key.Key_B, "", Qt.KeyboardModifier.ControlModifier)
    assert event.isAccepted()


def test_unrepresentable_unicode_is_not_truncated(display):
    events = []
    display.input_event.connect(lambda *args: events.append(args))
    _send(display, QEvent.Type.KeyPress, Qt.Key.Key_A, "Я")
    _send(display, QEvent.Type.KeyRelease, Qt.Key.Key_A)
    assert events[0][2] == 0  # SimInputEvent currently supports an 8-bit char.
