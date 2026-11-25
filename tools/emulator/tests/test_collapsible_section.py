from __future__ import annotations

import os

# Permit headless runs in CI environments without a display server.
os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

import pytest

pytest.importorskip("PySide6")

from PySide6.QtWidgets import QApplication, QGroupBox, QLabel, QScrollArea, QVBoxLayout, QWidget

from lexxos_emu.gui_widgets import CollapsibleSection


def test_collapsed_section_restores_content_and_height() -> None:
    app = QApplication.instance() or QApplication([])
    content = QGroupBox("Details")
    layout = QVBoxLayout(content)
    layout.addWidget(QLabel("A visible child widget"))

    section = CollapsibleSection("Details", content, expanded=False)
    root = QWidget()
    root_layout = QVBoxLayout(root)
    root_layout.addWidget(section)
    root_layout.addStretch()
    scroll = QScrollArea()
    scroll.setWidgetResizable(True)
    scroll.setWidget(root)
    scroll.resize(450, 350)
    scroll.show()
    app.processEvents()

    try:
        assert not content.isVisible()
        collapsed_height = section.sizeHint().height()
        section._toggle.click()
        app.processEvents()
        assert content.isVisible()
        assert section.sizeHint().height() > collapsed_height
        section._toggle.click()
        app.processEvents()
        assert not content.isVisible()
        section._toggle.click()
        app.processEvents()
        assert content.isVisible()
    finally:
        scroll.close()
