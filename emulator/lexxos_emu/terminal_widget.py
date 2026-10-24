"""Keyboard-driven ANSI terminal for the PySide6 emulator window."""

from __future__ import annotations

from PySide6.QtCore import QPointF, QRectF, Qt, Signal
from PySide6.QtGui import QColor, QFont, QFontMetricsF, QKeySequence, QPainter
from PySide6.QtWidgets import QApplication, QAbstractScrollArea

from .ansi_terminal import AnsiTerminal, DEFAULT_BG


class TerminalWidget(QAbstractScrollArea):
    input_bytes = Signal(bytes)

    def __init__(self, parent=None):
        super().__init__(parent)
        font = QFont("Menlo", 12)
        font.setStyleHint(QFont.StyleHint.Monospace)
        font.setFixedPitch(True)
        self.setFont(font)
        # Keep fractional font advances: rounding every cell to an integer
        # shifts the cursor away from glyphs on HiDPI/fractional-scaled screens.
        metrics = QFontMetricsF(font)
        self._cell_width = max(1.0, metrics.horizontalAdvance("M"))
        self._cell_height = max(1.0, metrics.lineSpacing())
        self._ascent = metrics.ascent()
        self._ascent = metrics.ascent()
        self._terminal = AnsiTerminal()
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)
        self.setVerticalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAsNeeded)
        self.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        self.verticalScrollBar().valueChanged.connect(lambda _value: self.viewport().update())
        self._resize_terminal()

    def _resize_terminal(self) -> None:
        cols = max(1, int(self.viewport().width() // self._cell_width))
        rows = max(1, int(self.viewport().height() // self._cell_height))
        self._terminal.resize(cols, rows)
        self._update_scrollbar(force_bottom=True)
        self.viewport().update()

    def resizeEvent(self, event):  # noqa: N802
        super().resizeEvent(event)
        self._resize_terminal()

    def _update_scrollbar(self, *, force_bottom: bool = False) -> None:
        bar = self.verticalScrollBar()
        was_at_bottom = bar.value() >= bar.maximum()
        bar.setRange(0, len(self._terminal.history))
        bar.setPageStep(self._terminal.rows)
        if force_bottom or was_at_bottom:
            bar.setValue(bar.maximum())

    def clear(self) -> None:
        self._terminal.clear()
        self._update_scrollbar(force_bottom=True)
        self.viewport().update()

    def feed(self, data: str) -> None:
        if not data:
            return
        self._terminal.feed(data)
        self._update_scrollbar()
        self.viewport().update()

    def _cursor_rect(self) -> QRectF:
        # A narrow insertion caret denotes the *boundary* before the next
        # character; a cell-sized outline covers glyphs when editing a line.
        # At the last cell VT-style delayed wrapping leaves the caret at the
        # right edge until another character arrives.
        column = self._terminal.col + (1 if self._terminal._wrap_pending else 0)
        x = min(column * self._cell_width, max(0.0, self.viewport().width() - 2.0))
        y = self._terminal.row * self._cell_height
        return QRectF(x, y + 1.0, 2.0, max(1.0, self._cell_height - 2.0))

    def paintEvent(self, event):  # noqa: N802
        painter = QPainter(self.viewport())
        painter.fillRect(event.rect(), QColor(*DEFAULT_BG))
        start = self.verticalScrollBar().value()
        lines = list(self._terminal.history) + self._terminal.screen
        painter.setFont(self.font())
        for y, cells in enumerate(lines[start:start + self._terminal.rows]):
            baseline = y * self._cell_height + self._ascent
            index = 0
            while index < len(cells):
                first = cells[index]
                end = index + 1
                while end < len(cells) and (cells[end].fg, cells[end].bg, cells[end].bold) == (first.fg, first.bg,
                                                                                               first.bold):
                    end += 1
                x = index * self._cell_width
                if first.bg != DEFAULT_BG:
                    painter.fillRect(
                        QRectF(x, y * self._cell_height, (end - index) * self._cell_width,
                               self._cell_height), QColor(*first.bg)
                    )
                painter.setPen(QColor(*first.fg))
                font = self.font()
                font.setBold(first.bold)
                painter.setFont(font)
                # Draw each glyph at its *cell* position. Drawing a whole text
                # run lets font shaping/rounded advances accumulate drift,
                # while the ANSI model always advances exactly one cell.
                for column in range(index, end):
                    char = cells[column].char
                    if char != " ":
                        painter.drawText(QPointF(column * self._cell_width, baseline), char)
                index = end
        if start == len(self._terminal.history) and self._terminal.cursor_visible and self.hasFocus():
            painter.fillRect(self._cursor_rect(), QColor(220, 220, 220))
        painter.end()

    def focusNextPrevChild(self, next):  # noqa: N802
        # Tab/Shift+Tab belong to the emulated UART, not Qt focus traversal.
        return False

    def mousePressEvent(self, event):  # noqa: N802
        self.setFocus()
        super().mousePressEvent(event)

    def keyPressEvent(self, event):  # noqa: N802
        if event.matches(QKeySequence.StandardKey.Paste):
            data = QApplication.clipboard().text().encode("utf-8")
            if data:
                self.input_bytes.emit(data[:8192])
            return

        mods = event.modifiers()
        ctrl = bool(mods & Qt.KeyboardModifier.ControlModifier)
        shift = bool(mods & Qt.KeyboardModifier.ShiftModifier)
        alt = bool(mods & Qt.KeyboardModifier.AltModifier)
        key = event.key()

        if ctrl and shift and key == Qt.Key.Key_C:
            QApplication.clipboard().setText(self._terminal.visible_text())
            return

        nav = {
            Qt.Key.Key_Up: b"\x1b[A",
            Qt.Key.Key_Down: b"\x1b[B",
            Qt.Key.Key_Right: b"\x1b[C",
            Qt.Key.Key_Left: b"\x1b[D",
            Qt.Key.Key_Home: b"\x1b[H",
            Qt.Key.Key_End: b"\x1b[F",
            Qt.Key.Key_Delete: b"\x1b[3~",
            Qt.Key.Key_Insert: b"\x1b[2~",
            Qt.Key.Key_PageUp: b"\x1b[5~",
            Qt.Key.Key_PageDown: b"\x1b[6~",
        }
        keys = {
            Qt.Key.Key_Return: b"\r",
            Qt.Key.Key_Enter: b"\r",
            Qt.Key.Key_Backspace: b"\x7f",
            Qt.Key.Key_Tab: b"\t",
            Qt.Key.Key_Backtab: b"\x1b[Z",
            Qt.Key.Key_Escape: b"\x1b",
            Qt.Key.Key_F3: b"\x1bOR",
            Qt.Key.Key_F5: b"\x1b[15~",
            Qt.Key.Key_F6: b"\x1b[17~",
            Qt.Key.Key_F8: b"\x1b[19~",
            Qt.Key.Key_F9: b"\x1b[20~",
            Qt.Key.Key_F10: b"\x1b[21~",
        }
        data = nav.get(key) or keys.get(key)
        if data is None and ctrl and Qt.Key.Key_A <= key <= Qt.Key.Key_Z:
            data = bytes((int(key) - int(Qt.Key.Key_A) + 1,))
        if data is None and ctrl and key == Qt.Key.Key_Space:
            data = b"\x00"
        if data is None and not ctrl:
            data = event.text().encode("utf-8")
        if not data:
            event.ignore()
            return
        if alt and data != b"\x1b":
            data = b"\x1b" + data
        self.verticalScrollBar().setValue(self.verticalScrollBar().maximum())
        self.input_bytes.emit(data)
        event.accept()

    def wheelEvent(self, event):  # noqa: N802
        steps = event.angleDelta().y() // 120
        bar = self.verticalScrollBar()
        bar.setValue(bar.value() - steps * 3)
        event.accept()
