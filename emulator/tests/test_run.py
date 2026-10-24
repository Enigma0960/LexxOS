from __future__ import annotations

from emulator.lexxos_emu.run import _decode_caret_escape_sequences


def test_decode_caret_escape_sequences():
    assert _decode_caret_escape_sequences("ls ^[[D^[[C^[[3~\n") == "ls \x1b[D\x1b[C\x1b[3~\n"


def test_normalize_newlines_for_raw_terminal_preserves_redraw_carriage_returns():
    from emulator.lexxos_emu.run import _normalize_newlines_for_raw_terminal
    assert _normalize_newlines_for_raw_terminal("a\nb") == "a\r\nb"
    assert _normalize_newlines_for_raw_terminal("a\r\nb") == "a\r\nb"
    assert _normalize_newlines_for_raw_terminal("\rlexx:/> ls\x1b[K") == "\rlexx:/> ls\x1b[K"
