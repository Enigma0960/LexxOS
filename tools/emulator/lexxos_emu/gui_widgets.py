from __future__ import annotations

try:
    from PySide6.QtCore import Qt
    from PySide6.QtWidgets import (
        QFrame,
        QSizePolicy,
        QSpinBox,
        QToolButton,
        QVBoxLayout,
        QWidget,
    )
except ImportError as exc:  # pragma: no cover - depends on optional GUI dependency.
    raise SystemExit("PySide6 is required for the GUI emulator: python -m pip install PySide6") from exc


class CollapsibleSection(QWidget):
    def __init__(
            self,
            title: str,
            content: QWidget,
            *,
            expanded: bool = True,
            parent: QWidget | None = None,
    ):
        super().__init__(parent)
        self._content = content

        self._toggle = QToolButton(self)
        self._toggle.setText(title)
        self._toggle.setCheckable(True)
        self._toggle.setChecked(expanded)
        self._toggle.setToolButtonStyle(Qt.ToolButtonStyle.ToolButtonTextBesideIcon)
        self._toggle.setArrowType(
            Qt.ArrowType.DownArrow if expanded else Qt.ArrowType.RightArrow
        )
        self._toggle.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        self._toggle.clicked.connect(self._set_expanded)

        self._content_frame = QFrame(self)
        self._content_frame.setFrameShape(QFrame.Shape.NoFrame)
        self._content.setParent(self._content_frame)

        content_layout = QVBoxLayout(self._content_frame)
        content_layout.setContentsMargins(0, 0, 0, 0)
        content_layout.addWidget(self._content)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(4)
        layout.addWidget(self._toggle)
        layout.addWidget(self._content_frame)

        # The content was reparented above; explicitly restore visibility.
        # An initially collapsed widget must become visible when expanded.
        self._set_expanded(expanded)

    def _set_expanded(self, expanded: bool) -> None:
        self._toggle.setArrowType(
            Qt.ArrowType.DownArrow if expanded else Qt.ArrowType.RightArrow
        )
        self._content_frame.setVisible(expanded)
        self._content.setVisible(expanded)


class HexSpinBox(QSpinBox):
    def __init__(self, parent: QWidget | None = None):
        super().__init__(parent)
        self.setRange(0x08, 0x77)
        self.setValue(0x3C)
        self.setDisplayIntegerBase(16)
        self.setPrefix("0x")

    def textFromValue(self, value: int) -> str:  # noqa: N802 - Qt override name.
        return f"{value:02X}"

    def valueFromText(self, text: str) -> int:  # noqa: N802 - Qt override name.
        normalized = text.strip().lower().removeprefix("0x")
        return int(normalized, 16)
