"""Small, dependency-free ANSI/VT100 screen model for the emulator GUI.

This is a character-cell terminal, not a shell or terminal process.  LexxOS owns
input editing and echo; this class only interprets its UART TX byte stream.
"""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass

RGB = tuple[int, int, int]

DEFAULT_FG: RGB = (224, 230, 238)
DEFAULT_BG: RGB = (18, 21, 29)

ANSI_COLORS: tuple[RGB, ...] = (
    (0, 0, 0), (205, 49, 49), (13, 188, 121), (229, 229, 16),
    (36, 114, 200), (188, 63, 188), (17, 168, 205), (229, 229, 229),
    (102, 102, 102), (241, 76, 76), (35, 209, 139), (245, 245, 67),
    (59, 142, 234), (214, 112, 214), (41, 184, 219), (255, 255, 255),
)


def indexed_color(index: int) -> RGB:
    if not 0 <= index <= 255:
        raise ValueError("ANSI palette index must be between 0 and 255")
    if index < 16:
        return ANSI_COLORS[index]
    if index < 232:
        i = index - 16
        levels = (0, 95, 135, 175, 215, 255)
        return (levels[i // 36], levels[(i // 6) % 6], levels[i % 6])
    grey = 8 + (index - 232) * 10
    return grey, grey, grey


@dataclass(frozen=True, slots=True)
class Cell:
    char: str = " "
    fg: RGB = DEFAULT_FG
    bg: RGB = DEFAULT_BG
    bold: bool = False


BLANK = Cell()


class AnsiTerminal:
    """Incremental ANSI parser with a scrollback buffer and fixed-size screen."""

    def __init__(self, cols: int = 80, rows: int = 25, scrollback: int = 2000):
        self.cols = max(1, int(cols))
        self.rows = max(1, int(rows))
        self.history: deque[list[Cell]] = deque(maxlen=max(0, scrollback))
        self.screen = [self._blank_row() for _ in range(self.rows)]
        self.row = 0
        self.col = 0
        self.cursor_visible = True
        self._wrap_pending = False
        self._saved_cursor = (0, 0)
        self._fg = DEFAULT_FG
        self._bg = DEFAULT_BG
        self._bold = False
        self._inverse = False
        self._state = "normal"
        self._sequence = ""
        self._alt_screen: tuple[list[list[Cell]], int, int] | None = None

    def _blank_row(self) -> list[Cell]:
        return [BLANK] * self.cols

    def clear(self) -> None:
        self.history.clear()
        self.screen = [self._blank_row() for _ in range(self.rows)]
        self.row = self.col = 0
        self._wrap_pending = False
        self._saved_cursor = (0, 0)
        self._fg, self._bg = DEFAULT_FG, DEFAULT_BG
        self._bold = self._inverse = False
        self.cursor_visible = True
        self._state = "normal"
        self._sequence = ""
        self._alt_screen = None

    def resize(self, cols: int, rows: int) -> None:
        cols, rows = max(1, int(cols)), max(1, int(rows))
        if (cols, rows) == (self.cols, self.rows):
            return
        old_rows = self.rows
        if self._alt_screen is not None:
            main, main_row, main_col = self._alt_screen
            main = [(line[:cols] + [BLANK] * max(0, cols - len(line))) for line in main]
            if rows < old_rows:
                main = main[old_rows - rows:]
                main_row -= old_rows - rows
            elif rows > old_rows:
                main.extend([BLANK] * cols for _ in range(rows - old_rows))
            self._alt_screen = (main, max(0, min(rows - 1, main_row)), min(cols - 1, main_col))
        self.cols = cols
        self.screen = [(line[:cols] + [BLANK] * max(0, cols - len(line))) for line in self.screen]
        if rows < self.rows:
            removed = self.screen[:self.rows - rows]
            if self._alt_screen is None:
                self.history.extend(removed)
            self.screen = self.screen[self.rows - rows:]
            self.row -= self.rows - rows
        elif rows > self.rows:
            # Grow below the screen; previously drawn cursor coordinates stay stable.
            self.screen.extend(self._blank_row() for _ in range(rows - self.rows))
        self.rows = rows
        self.row = min(max(0, self.row), rows - 1)
        self.col = min(self.col, cols - 1)
        self._wrap_pending = False

    def feed(self, text: str) -> None:
        for ch in text:
            if self._state == "normal":
                if ch == "\x1b":
                    self._state = "esc"
                elif ch == "\r":
                    self.col = 0
                    self._wrap_pending = False
                elif ch in ("\n", "\x0b", "\x0c"):
                    self._linefeed()
                    # LexxOS's console uses LF as an output newline.
                    self.col = 0
                    self._wrap_pending = False
                elif ch == "\b":
                    self.col = max(0, self.col - 1)
                    self._wrap_pending = False
                elif ch == "\t":
                    self._tab()
                elif ch >= " " and ch != "\x7f":
                    self._print(ch)
            elif self._state == "esc":
                self._state = "normal"
                if ch == "[":
                    self._state, self._sequence = "csi", ""
                elif ch == "]":
                    self._state = "osc"
                elif ch == "7":
                    self._saved_cursor = self.row, self.col
                elif ch == "8":
                    self.row, self.col = self._saved_cursor
                    self._clamp_cursor()
                elif ch == "D":
                    self._linefeed()
                elif ch == "E":
                    self._linefeed()
                    self.col = 0
                elif ch == "M":
                    self._reverse_index()
                elif ch == "c":
                    self.clear()
            elif self._state == "csi":
                if "@" <= ch <= "~":
                    self._execute_csi(self._sequence, ch)
                    self._state, self._sequence = "normal", ""
                elif " " <= ch <= "?" and len(self._sequence) < 96:
                    self._sequence += ch
                else:
                    # Consume the rest of a malformed/oversized CSI to its final byte.
                    self._state, self._sequence = "csi_ignore", ""
            elif self._state == "csi_ignore":
                if "@" <= ch <= "~":
                    self._state = "normal"
            elif self._state == "osc":
                if ch == "\x07":
                    self._state = "normal"
                elif ch == "\x1b":
                    self._state = "osc_esc"
            elif self._state == "osc_esc":
                self._state = "normal" if ch == "\\" else "osc"

    def _print(self, ch: str) -> None:
        if self._wrap_pending:
            self._linefeed()
            self.col = 0
            self._wrap_pending = False
        fg, bg = (self._bg, self._fg) if self._inverse else (self._fg, self._bg)
        self.screen[self.row][self.col] = Cell(ch, fg, bg, self._bold)
        if self.col == self.cols - 1:
            self._wrap_pending = True
        else:
            self.col += 1

    def _tab(self) -> None:
        target = min(self.cols, ((self.col // 8) + 1) * 8)
        for _ in range(target - self.col):
            self._print(" ")

    def _linefeed(self) -> None:
        self._wrap_pending = False
        if self.row == self.rows - 1:
            removed = self.screen.pop(0)
            if self._alt_screen is None:
                self.history.append(removed)
            self.screen.append(self._blank_row())
        else:
            self.row += 1

    def _reverse_index(self) -> None:
        if self.row == 0:
            self.screen.pop()
            self.screen.insert(0, self._blank_row())
        else:
            self.row -= 1
        self._wrap_pending = False

    def _clamp_cursor(self) -> None:
        self.row = min(self.rows - 1, max(0, self.row))
        self.col = min(self.cols - 1, max(0, self.col))
        self._wrap_pending = False

    @staticmethod
    def _number(params: list[int], index: int, default: int = 1) -> int:
        value = params[index] if index < len(params) else 0
        return value if value else default

    def _execute_csi(self, text: str, code: str) -> None:
        private = text.startswith("?")
        if private:
            text = text[1:]
        if text and any(not (c.isdigit() or c == ";") for c in text):
            return
        raw = text.split(";") if text else []
        params = [int(p or "0") for p in raw]
        if private:
            if code in ("h", "l"):
                if 25 in params:
                    self.cursor_visible = code == "h"
                if 1049 in params:
                    self._alternate_screen(code == "h")
            return
        count = self._number(params, 0)
        if code == "A":
            self.row -= count
        elif code == "B":
            self.row += count
        elif code == "C":
            self.col += count
        elif code == "D":
            self.col -= count
        elif code == "E":
            self.row += count
            self.col = 0
        elif code == "F":
            self.row -= count
            self.col = 0
        elif code == "G":
            self.col = count - 1
        elif code == "d":
            self.row = count - 1
        elif code in ("H", "f"):
            self.row = count - 1
            self.col = self._number(params, 1) - 1
        elif code == "s":
            self._saved_cursor = self.row, self.col
        elif code == "u":
            self.row, self.col = self._saved_cursor
        elif code == "K":
            mode = params[0] if params else 0
            start, stop = ((self.col, self.cols) if mode == 0 else
                           (0, self.col + 1) if mode == 1 else (0, self.cols))
            if mode in (0, 1, 2):
                self._erase_segment(self.row, start, stop)
        elif code == "J":
            mode = params[0] if params else 0
            if mode == 0:
                self._erase_segment(self.row, self.col, self.cols)
                for r in range(self.row + 1, self.rows):
                    self._erase_segment(r, 0, self.cols)
            elif mode == 1:
                for r in range(self.row):
                    self._erase_segment(r, 0, self.cols)
                self._erase_segment(self.row, 0, self.col + 1)
            elif mode == 2:
                for r in range(self.rows):
                    self._erase_segment(r, 0, self.cols)
            elif mode == 3:
                self.history.clear()
        elif code == "X":
            self._erase_segment(self.row, self.col, min(self.cols, self.col + count))
        elif code == "P":
            line = self.screen[self.row]
            del line[self.col:self.col + count]
            line.extend(BLANK for _ in range(self.cols - len(line)))
        elif code == "@":
            line = self.screen[self.row]
            line[self.col:self.col] = [BLANK] * min(count, self.cols)
            del line[self.cols:]
        elif code == "S":
            for _ in range(min(count, self.rows)):
                removed = self.screen.pop(0)
                if self._alt_screen is None:
                    self.history.append(removed)
                self.screen.append(self._blank_row())
        elif code == "T":
            for _ in range(min(count, self.rows)):
                self.screen.pop()
                self.screen.insert(0, self._blank_row())
        elif code == "m":
            self._sgr(params or [0])
        self._clamp_cursor()

    def _erase_segment(self, row: int, start: int, stop: int) -> None:
        fg, bg = (self._bg, self._fg) if self._inverse else (self._fg, self._bg)
        blank = Cell(" ", fg, bg, self._bold)
        self.screen[row][max(0, start):min(stop, self.cols)] = [blank] * max(0, min(stop, self.cols) - max(0, start))

    def _alternate_screen(self, enabled: bool) -> None:
        if enabled and self._alt_screen is None:
            self._alt_screen = (self.screen, self.row, self.col)
            self.screen = [self._blank_row() for _ in range(self.rows)]
            self.row = self.col = 0
        elif not enabled and self._alt_screen is not None:
            self.screen, self.row, self.col = self._alt_screen
            self._alt_screen = None
            self._clamp_cursor()
        self._wrap_pending = False

    def _sgr(self, params: list[int]) -> None:
        i = 0
        while i < len(params):
            p = params[i]
            if p == 0:
                self._fg, self._bg = DEFAULT_FG, DEFAULT_BG
                self._bold = self._inverse = False
            elif p == 1:
                self._bold = True
            elif p == 22:
                self._bold = False
            elif p == 7:
                self._inverse = True
            elif p == 27:
                self._inverse = False
            elif p == 39:
                self._fg = DEFAULT_FG
            elif p == 49:
                self._bg = DEFAULT_BG
            elif 30 <= p <= 37:
                self._fg = ANSI_COLORS[p - 30]
            elif 40 <= p <= 47:
                self._bg = ANSI_COLORS[p - 40]
            elif 90 <= p <= 97:
                self._fg = ANSI_COLORS[p - 90 + 8]
            elif 100 <= p <= 107:
                self._bg = ANSI_COLORS[p - 100 + 8]
            elif p in (38, 48) and i + 2 < len(params):
                color: RGB | None = None
                consumed = 0
                if params[i + 1] == 5 and 0 <= params[i + 2] <= 255:
                    color, consumed = indexed_color(params[i + 2]), 2
                elif params[i + 1] == 2 and i + 4 < len(params):
                    rgb = params[i + 2:i + 5]
                    if all(0 <= v <= 255 for v in rgb):
                        color, consumed = (rgb[0], rgb[1], rgb[2]), 4
                if color is not None:
                    if p == 38:
                        self._fg = color
                    else:
                        self._bg = color
                i += consumed
            i += 1

    def visible_text(self) -> str:
        return "\n".join("".join(cell.char for cell in row).rstrip() for row in self.screen)
