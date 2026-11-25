from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum


class LexxOsSimError(RuntimeError):
    pass


@dataclass(frozen=True)
class DisplayInfo:
    width: int
    height: int
    format: int
    frame_counter: int


class KeyCode(IntEnum):
    UNKNOWN = 0
    UP = 1
    DOWN = 2
    LEFT = 3
    RIGHT = 4
    ENTER = 5
    ESCAPE = 6
    BACKSPACE = 7
    SPACE = 8
    A = 9
    B = 10
    C = 11
    D = 12
    X = 13
    Y = 14
    Z = 15
    NUM0 = 16
    NUM1 = 17
    NUM2 = 18
    NUM3 = 19
    NUM4 = 20
    NUM5 = 21
    NUM6 = 22
    NUM7 = 23
    NUM8 = 24
    NUM9 = 25
    SOFT1 = 26
    SOFT2 = 27
    STAR = 28
    SHARP = 29
    POWER = 30
    RESERVED = 31
    E = 32
    F = 33
    G = 34
    H = 35
    I = 36
    J = 37
    K = 38
    L = 39
    M = 40
    N = 41
    O = 42
    P = 43
    Q = 44
    R = 45
    S = 46
    T = 47
    U = 48
    V = 49
    W = 50
    TAB = 51
    GRAVE = 52
    MINUS = 53
    EQUALS = 54
    LEFT_BRACKET = 55
    RIGHT_BRACKET = 56
    BACKSLASH = 57
    SEMICOLON = 58
    APOSTROPHE = 59
    COMMA = 60
    PERIOD = 61
    SLASH = 62
    F1 = 63
    F2 = 64
    F3 = 65
    F4 = 66
    F5 = 67
    F6 = 68
    F7 = 69
    F8 = 70
    F9 = 71
    F10 = 72
    BREAK = 73
    HOME = 74
    END = 75
    DELETE = 76
    INSERT = 77


class InputModifier(IntEnum):
    ALT = 1 << 0
    SHIFT_LEFT = 1 << 1
    SHIFT_RIGHT = 1 << 2
    SYM = 1 << 3
    CTRL = 1 << 4


KEY_ALIASES: dict[str, KeyCode] = {
    "unknown": KeyCode.UNKNOWN,
    "up": KeyCode.UP,
    "down": KeyCode.DOWN,
    "left": KeyCode.LEFT,
    "right": KeyCode.RIGHT,
    "enter": KeyCode.ENTER,
    "return": KeyCode.ENTER,
    "escape": KeyCode.ESCAPE,
    "esc": KeyCode.ESCAPE,
    "backspace": KeyCode.BACKSPACE,
    "space": KeyCode.SPACE,
    "a": KeyCode.A,
    "b": KeyCode.B,
    "c": KeyCode.C,
    "d": KeyCode.D,
    "e": KeyCode.E,
    "f": KeyCode.F,
    "g": KeyCode.G,
    "h": KeyCode.H,
    "i": KeyCode.I,
    "j": KeyCode.J,
    "k": KeyCode.K,
    "l": KeyCode.L,
    "m": KeyCode.M,
    "n": KeyCode.N,
    "o": KeyCode.O,
    "p": KeyCode.P,
    "q": KeyCode.Q,
    "r": KeyCode.R,
    "s": KeyCode.S,
    "t": KeyCode.T,
    "u": KeyCode.U,
    "v": KeyCode.V,
    "w": KeyCode.W,
    "x": KeyCode.X,
    "y": KeyCode.Y,
    "z": KeyCode.Z,
    "0": KeyCode.NUM0,
    "1": KeyCode.NUM1,
    "2": KeyCode.NUM2,
    "3": KeyCode.NUM3,
    "4": KeyCode.NUM4,
    "5": KeyCode.NUM5,
    "6": KeyCode.NUM6,
    "7": KeyCode.NUM7,
    "8": KeyCode.NUM8,
    "9": KeyCode.NUM9,
    "soft1": KeyCode.SOFT1,
    "soft2": KeyCode.SOFT2,
    "star": KeyCode.STAR,
    "*": KeyCode.STAR,
    "sharp": KeyCode.SHARP,
    "#": KeyCode.SHARP,
    "power": KeyCode.POWER,
    "tab": KeyCode.TAB,
    "grave": KeyCode.GRAVE,
    "`": KeyCode.GRAVE,
    "minus": KeyCode.MINUS,
    "-": KeyCode.MINUS,
    "equals": KeyCode.EQUALS,
    "=": KeyCode.EQUALS,
    "left_bracket": KeyCode.LEFT_BRACKET,
    "[": KeyCode.LEFT_BRACKET,
    "right_bracket": KeyCode.RIGHT_BRACKET,
    "]": KeyCode.RIGHT_BRACKET,
    "backslash": KeyCode.BACKSLASH,
    "\\": KeyCode.BACKSLASH,
    "semicolon": KeyCode.SEMICOLON,
    ";": KeyCode.SEMICOLON,
    "apostrophe": KeyCode.APOSTROPHE,
    "'": KeyCode.APOSTROPHE,
    "comma": KeyCode.COMMA,
    ",": KeyCode.COMMA,
    "period": KeyCode.PERIOD,
    ".": KeyCode.PERIOD,
    "slash": KeyCode.SLASH,
    "/": KeyCode.SLASH,
    "f1": KeyCode.F1,
    "f2": KeyCode.F2,
    "f3": KeyCode.F3,
    "f4": KeyCode.F4,
    "f5": KeyCode.F5,
    "f6": KeyCode.F6,
    "f7": KeyCode.F7,
    "f8": KeyCode.F8,
    "f9": KeyCode.F9,
    "f10": KeyCode.F10,
    "break": KeyCode.BREAK,
    "home": KeyCode.HOME,
    "end": KeyCode.END,
    "delete": KeyCode.DELETE,
    "del": KeyCode.DELETE,
    "insert": KeyCode.INSERT,
    "ins": KeyCode.INSERT,
}
