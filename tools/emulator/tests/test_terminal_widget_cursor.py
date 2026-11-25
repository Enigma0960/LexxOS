"""GUI cursor regression tests; execute headlessly with PySide6 installed."""

import os

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

import pytest

pytest.importorskip("PySide6")

from PySide6.QtGui import QFontMetricsF
from PySide6.QtWidgets import QApplication

from lexxos_emu.terminal_widget import TerminalWidget


@pytest.fixture
def terminal():
    app = QApplication.instance() or QApplication([])
    widget = TerminalWidget()
    widget.resize(480, 160)
    widget.show()
    app.processEvents()
    yield widget
    widget.close()
    app.processEvents()


def test_cursor_is_thin_and_uses_fractional_cell_width(terminal):
    terminal.feed("lexx:/> test")
    caret = terminal._cursor_rect()
    assert caret.width() <= 2.0
    assert caret.x() == pytest.approx(terminal._terminal.col * terminal._cell_width)
    assert terminal._cell_width == pytest.approx(QFontMetricsF(terminal.font()).horizontalAdvance("M"))
    assert not terminal.grab().isNull()  # QPainter renders the glyphs and caret.


def test_left_arrow_moves_caret_without_overwriting_last_character(terminal):
    terminal.feed("test")
    last_column = terminal._terminal.col
    terminal.feed("\x1b[D")
    assert terminal._terminal.col == last_column - 1
    assert terminal._terminal.screen[0][last_column - 1].char == "t"
    assert terminal._cursor_rect().x() == pytest.approx((last_column - 1) * terminal._cell_width)


def test_cursor_is_inside_viewport_after_delayed_wrap(terminal):
    terminal.feed("X" * terminal._terminal.cols)
    assert terminal._terminal._wrap_pending
    caret = terminal._cursor_rect()
    assert caret.right() <= terminal.viewport().width()
    assert caret.y() >= 0
