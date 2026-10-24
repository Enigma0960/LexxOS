#!/usr/bin/env python3
"""Compatibility launcher for LexxCP."""

from __future__ import annotations

from pathlib import Path
import runpy
import sys

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
runpy.run_module("lexxcp", run_name="__main__")
