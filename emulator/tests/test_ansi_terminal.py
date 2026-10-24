from __future__ import annotations

from lexxos_emu.ansi_terminal import ANSI_COLORS, AnsiTerminal, indexed_color


def row(term: AnsiTerminal, index: int = 0) -> str:
    return "".join(cell.char for cell in term.screen[index])


def test_streaming_escape_sequence_and_color_reset():
    t = AnsiTerminal(20, 3)
    t.feed("one \x1b[3")
    assert row(t).startswith("one ")
    t.feed("1mred\x1b[0m normal")
    assert row(t).startswith("one red normal")
    assert t.screen[0][4].fg == ANSI_COLORS[1]
    assert t.screen[0][8].fg != ANSI_COLORS[1]


def test_csi_cursor_position_and_overwrite():
    t = AnsiTerminal(10, 4)
    t.feed("hello\rHI")
    assert row(t).startswith("HIllo")
    t.feed("\x1b[2;3HXYZ")
    assert row(t, 1)[2:5] == "XYZ"
    t.feed("\x1b[1;1H\x1b[4C!")
    assert row(t).startswith("HIll!")


def test_clear_screen_and_line_modes():
    t = AnsiTerminal(10, 3)
    t.feed("ABCDEF\x1b[1;3H\x1b[K")
    assert row(t) == "AB" + " " * 8
    t.feed("\x1b[2;1Hhello\x1b[2J")
    assert t.visible_text().strip() == ""
    assert (t.row, t.col) == (1, 5)  # ED does not move cursor


def test_scrollback_wrap_and_resize():
    t = AnsiTerminal(5, 2, scrollback=2)
    t.feed("aaaaa")
    assert t.col == 4
    t.feed("b")
    assert row(t, 0) == "aaaaa"
    assert row(t, 1).startswith("b")
    t.feed("\r\nsecond\nthird")
    assert len(t.history) == 2
    t.resize(8, 3)
    assert all(len(line) == 8 for line in t.screen)


def test_extended_palette_rgb_background_inverse():
    t = AnsiTerminal(25, 1)
    t.feed("\x1b[38;5;196;48;2;3;4;5mX\x1b[7mY\x1b[0mZ")
    assert t.screen[0][0].fg == indexed_color(196)
    assert t.screen[0][0].bg == (3, 4, 5)
    assert t.screen[0][1].fg == (3, 4, 5)
    assert t.screen[0][2].fg != (3, 4, 5)


def test_cursor_visibility_and_alt_screen():
    t = AnsiTerminal(10, 2)
    t.feed("main\x1b[?25l\x1b[?1049hOTHER")
    assert not t.cursor_visible
    assert row(t).startswith("OTHER")
    t.feed("\x1b[?1049l\x1b[?25h")
    assert row(t).startswith("main")
    assert t.cursor_visible


def test_ignores_osc_sequences_and_oversized_csi():
    t = AnsiTerminal(15, 2)
    t.feed("A\x1b]0;secret\x07B\x1b[" + "1" * 120 + "mC")
    assert row(t).startswith("ABC")


def test_alt_screen_survives_resize():
    t = AnsiTerminal(8, 2)
    t.feed('original\x1b[?1049h')
    t.resize(12, 3)
    t.feed('other\x1b[?1049l')
    assert row(t).startswith('original')
    assert all(len(line) == 12 for line in t.screen)
