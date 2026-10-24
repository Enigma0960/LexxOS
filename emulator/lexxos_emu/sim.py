"""Compatibility facade for the public simulator API."""

from .sim_abi import (
    BLOCK_COUNT_CB,
    BLOCK_IS_INSERTED_CB,
    BLOCK_READ_CB,
    BLOCK_WRITE_CB,
    AUDIO_DRAIN_CB,
    AUDIO_PCM_CB,
    AUDIO_STOP_CB,
    AUDIO_TONE_CB,
    I2C_PROBE_CB,
    I2C_WRITE_CB,
    LOG_CB,
    NOW_US_CB,
    LexxOsSimBoardApi,
)
from .sim_session import LexxOsSim
from .sim_types import DisplayInfo, InputModifier, KEY_ALIASES, KeyCode, LexxOsSimError

__all__ = [
    "BLOCK_COUNT_CB",
    "BLOCK_IS_INSERTED_CB",
    "BLOCK_READ_CB",
    "BLOCK_WRITE_CB",
    "AUDIO_DRAIN_CB",
    "AUDIO_PCM_CB",
    "AUDIO_STOP_CB",
    "AUDIO_TONE_CB",
    "DisplayInfo",
    "InputModifier",
    "I2C_PROBE_CB",
    "I2C_WRITE_CB",
    "KEY_ALIASES",
    "KeyCode",
    "LOG_CB",
    "LexxOsSim",
    "LexxOsSimBoardApi",
    "LexxOsSimError",
    "NOW_US_CB",
]
