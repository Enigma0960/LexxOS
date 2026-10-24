from __future__ import annotations

from dataclasses import dataclass

from .sim import KeyCode


@dataclass(frozen=True)
class GpioInputSpec:
    pin: int
    title: str
    default_high: bool = False


@dataclass(frozen=True)
class SimKeySpec:
    key: KeyCode
    title: str


DEFAULT_SIM_KEYS: tuple[SimKeySpec, ...] = (
    SimKeySpec(KeyCode.UP, "D-Pad Up"),
    SimKeySpec(KeyCode.DOWN, "D-Pad Down"),
    SimKeySpec(KeyCode.LEFT, "D-Pad Left"),
    SimKeySpec(KeyCode.RIGHT, "D-Pad Right"),
    SimKeySpec(KeyCode.Z, "A button / Z"),
    SimKeySpec(KeyCode.X, "B button / X"),
    SimKeySpec(KeyCode.ENTER, "Start / Enter"),
    SimKeySpec(KeyCode.BACKSPACE, "Select / Backspace"),
)

