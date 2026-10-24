from __future__ import annotations

from pathlib import Path

from .board import VirtualBoard
from .fat32_export import export_fat32_snapshot
from .gui_models import DEFAULT_SIM_KEYS
from .gui_widgets import CollapsibleSection, HexSpinBox
from .terminal_widget import TerminalWidget
from .sim import KeyCode, LexxOsSim, LexxOsSimError

try:
    from PySide6.QtCore import QTimer, Qt
    from PySide6.QtGui import (
        QAction,
        QCloseEvent,
        QImage,
        QKeySequence,
        QPixmap,
    )
    from PySide6.QtWidgets import (
        QCheckBox,
        QFileDialog,
        QFormLayout,
        QGroupBox,
        QComboBox,
        QHBoxLayout,
        QLabel,
        QLineEdit,
        QInputDialog,
        QListWidget,
        QMainWindow,
        QMessageBox,
        QPushButton,
        QScrollArea,
        QSplitter,
        QStatusBar,
        QToolBar,
        QVBoxLayout,
        QWidget,
    )
except ImportError as exc:  # pragma: no cover - depends on optional GUI dependency.
    raise SystemExit("PySide6 is required for the GUI emulator: python -m pip install PySide6") from exc


class LexxOsEmulatorWindow(QMainWindow):
    def __init__(
            self,
            *,
            library_path: Path | None = None,
            sd_image: Path | None = None,
            sd_directory: Path | None = None,
            sd_capacity: str = "16G",
            sd_filesystem: str = "fat32",
            sd_writable: bool = False,
            oled: bool = False,
            audio_mode: str = "pwm",
            audio_output: str = "auto",
            audio_wav_path: Path | None = None,
    ):
        super().__init__()
        self.setWindowTitle("LexxOS Emulator")
        self.resize(1180, 760)

        self._sim: LexxOsSim | None = None
        self._board: VirtualBoard | None = None
        self._booted = False
        self._last_display_frame = -1

        self._run_timer = QTimer(self)
        self._run_timer.setInterval(20)
        self._run_timer.timeout.connect(self._tick)

        self._library_path = library_path
        self._sd_image = sd_image
        self._sd_directory = sd_directory
        self._sd_capacity = sd_capacity
        self._sd_filesystem = sd_filesystem
        self._sd_writable = sd_writable

        self._audio_mode = audio_mode
        self._audio_output = audio_output
        self._audio_wav_path = audio_wav_path or Path("lexxos-audio.wav")

        self._build_actions()
        self._build_ui(oled=oled)
        self._update_buttons()

    def closeEvent(self, event: QCloseEvent) -> None:  # noqa: N802 - Qt override name.
        self._close_simulator()
        event.accept()

    def _build_actions(self) -> None:
        toolbar = QToolBar("Emulator", self)
        toolbar.setMovable(False)
        self.addToolBar(toolbar)

        self._boot_action = QAction("Boot", self)
        self._boot_action.setShortcut(QKeySequence("Ctrl+B"))
        self._boot_action.triggered.connect(self._boot)
        toolbar.addAction(self._boot_action)

        self._shutdown_action = QAction("Shutdown", self)
        self._shutdown_action.triggered.connect(self._shutdown)
        toolbar.addAction(self._shutdown_action)

        self._reset_action = QAction("Reset", self)
        self._reset_action.triggered.connect(self._reset)
        toolbar.addAction(self._reset_action)

        toolbar.addSeparator()

        self._scan_action = QAction("Run i2c-scan", self)
        self._scan_action.triggered.connect(self._run_i2c_scan)
        toolbar.addAction(self._scan_action)

        self.setStatusBar(QStatusBar(self))

    def _build_ui(self, *, oled: bool) -> None:
        root = QSplitter(Qt.Orientation.Horizontal, self)
        self.setCentralWidget(root)

        controls_scroll = QScrollArea(root)
        controls_scroll.setWidgetResizable(True)
        controls_scroll.setVerticalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAsNeeded)
        controls_scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        controls_scroll.setMinimumWidth(360)
        controls_scroll.setMaximumWidth(520)

        controls = QWidget(controls_scroll)
        controls_layout = QVBoxLayout(controls)
        controls_layout.setContentsMargins(0, 0, 0, 0)
        controls_layout.setSpacing(8)

        controls_layout.addWidget(self._make_collapsible_section(self._create_paths_group(), expanded=True))
        controls_layout.addWidget(self._make_collapsible_section(self._create_display_group(), expanded=True))
        controls_layout.addWidget(self._make_collapsible_section(self._create_audio_group(), expanded=True))
        controls_layout.addWidget(self._make_collapsible_section(self._create_storage_group(), expanded=False))
        controls_layout.addWidget(self._make_collapsible_section(self._create_sim_input_group(), expanded=False))
        controls_layout.addWidget(self._make_collapsible_section(self._create_i2c_group(oled=oled), expanded=False))

        controls_layout.addStretch(1)

        controls_scroll.setWidget(controls)

        terminal_panel = QWidget(root)
        terminal_layout = QVBoxLayout(terminal_panel)
        terminal_layout.setContentsMargins(0, 0, 0, 0)

        self._terminal = TerminalWidget(terminal_panel)
        self._terminal.input_bytes.connect(self._send_terminal_input)
        terminal_layout.addWidget(self._terminal, 1)

        # Only QScrollArea is a splitter child. `controls` belongs to its viewport.
        # Adding `controls` directly to QSplitter reparents it out of QScrollArea.
        root.addWidget(controls_scroll)
        root.addWidget(terminal_panel)
        root.setStretchFactor(0, 0)
        root.setStretchFactor(1, 1)
        root.setSizes([400, 780])

    def _make_collapsible_section(self, widget: QWidget, *, expanded: bool) -> CollapsibleSection:
        title = "Section"
        if isinstance(widget, QGroupBox):
            title = widget.title() or title
            widget.setTitle("")
        return CollapsibleSection(title, widget, expanded=expanded, parent=self)

    def _create_paths_group(self) -> QGroupBox:
        group = QGroupBox("Simulator", self)
        layout = QFormLayout(group)

        self._lib_path_edit = QLineEdit(group)
        self._lib_path_edit.setText(str(self._library_path or ""))
        self._lib_path_edit.setPlaceholderText("build/sim/src/liblexxos_sim.dylib или .so")
        lib_browse = QPushButton("Browse…", group)
        lib_browse.clicked.connect(self._browse_library)
        lib_row = QHBoxLayout()
        lib_row.addWidget(self._lib_path_edit, 1)
        lib_row.addWidget(lib_browse)
        layout.addRow("Library", lib_row)

        self._sd_mode_combo = QComboBox(group)
        self._sd_mode_combo.addItem("Raw/FAT image", "image")
        self._sd_mode_combo.addItem("Host directory", "directory")
        if self._sd_directory is not None:
            self._sd_mode_combo.setCurrentIndex(1)
        self._sd_mode_combo.currentIndexChanged.connect(self._update_sd_storage_mode)
        layout.addRow("SD source", self._sd_mode_combo)

        self._sd_path_edit = QLineEdit(group)
        self._sd_path_edit.setText(str(self._sd_directory or self._sd_image or ""))
        self._sd_browse_button = QPushButton("Browse…", group)
        self._sd_browse_button.clicked.connect(self._browse_sd_storage)
        sd_row = QHBoxLayout()
        sd_row.addWidget(self._sd_path_edit, 1)
        sd_row.addWidget(self._sd_browse_button)
        layout.addRow("SD path", sd_row)

        self._capacity_combo = QComboBox(group)
        for value in ("256M", "512M", "16G", "32G"):
            self._capacity_combo.addItem(value, value)
        self._capacity_combo.setCurrentIndex(max(0, self._capacity_combo.findData(self._sd_capacity)))
        layout.addRow("SD capacity", self._capacity_combo)

        self._filesystem_combo = QComboBox(group)
        self._filesystem_combo.addItem("FAT32", "fat32")
        self._filesystem_combo.addItem("FAT16 (small images)", "fat16")
        self._filesystem_combo.setCurrentIndex(max(0, self._filesystem_combo.findData(self._sd_filesystem)))
        layout.addRow("Filesystem", self._filesystem_combo)

        self._update_sd_storage_mode()

        return group

    def _create_storage_group(self) -> QGroupBox:
        group = QGroupBox("SD card", self)
        layout = QVBoxLayout(group)

        self._sd_inserted_check = QCheckBox("Inserted / GP22 active-low detect", group)
        self._sd_inserted_check.setChecked(bool(self._sd_image or self._sd_directory))
        self._sd_inserted_check.stateChanged.connect(self._apply_sd_inserted)
        layout.addWidget(self._sd_inserted_check)

        self._sd_writable_check = QCheckBox("Writable persistent image (changes are permanent)", group)
        self._sd_writable_check.setChecked(self._sd_writable)
        layout.addWidget(self._sd_writable_check)
        self._export_snapshot_button = QPushButton("Export SD snapshot to new folder…", group)
        self._export_snapshot_button.clicked.connect(self._export_sd_snapshot)
        layout.addWidget(self._export_snapshot_button)
        self._update_sd_storage_mode()

        return group

    def _create_audio_group(self) -> QGroupBox:
        group = QGroupBox("Audio", self)
        layout = QFormLayout(group)
        self._audio_mode_combo = QComboBox(group)
        for label, mode in (("PWM audio", "pwm"), ("ADC audio", "adc"), ("Disabled", "off")):
            self._audio_mode_combo.addItem(label, mode)
        self._audio_mode_combo.setCurrentIndex(max(0, self._audio_mode_combo.findData(self._audio_mode)))
        layout.addRow("Device", self._audio_mode_combo)

        self._audio_output_combo = QComboBox(group)
        for label, mode in (("Auto (host speakers)", "auto"), ("On (required)", "on"),
                            ("Off (silent)", "off"), ("WAV file", "wav")):
            self._audio_output_combo.addItem(label, mode)
        self._audio_output_combo.setCurrentIndex(max(0, self._audio_output_combo.findData(self._audio_output)))
        layout.addRow("Host output", self._audio_output_combo)

        self._audio_wav_path_edit = QLineEdit(group)
        self._audio_wav_path_edit.setText(str(self._audio_wav_path))
        layout.addRow("WAV path", self._audio_wav_path_edit)
        self._audio_output_combo.currentIndexChanged.connect(self._update_audio_wav_controls)

        self._audio_status = QLabel("Audio is not running", group)
        self._audio_status.setWordWrap(True)
        layout.addRow("Status", self._audio_status)
        self._update_audio_wav_controls()
        return group

    def _update_audio_wav_controls(self, _index: int | None = None) -> None:
        wav_selected = self._audio_output_combo.currentData() == "wav"
        self._audio_wav_path_edit.setEnabled(wav_selected and self._sim is None)

    def _update_audio_status(self) -> None:
        if self._board is None:
            self._audio_status.setText("Audio is not running")
        elif self._audio_mode_combo.currentData() == "off":
            self._audio_status.setText("Emulated audio device disabled")
        elif self._audio_output_combo.currentData() == "off":
            self._audio_status.setText("Silent: audio requests are recorded, not played")
        elif self._board.audio.host_output_available:
            if self._audio_output_combo.currentData() == "wav":
                self._audio_status.setText("Recording audio to WAV file")
            else:
                self._audio_status.setText("Host audio available (sounddevice)")
        else:
            reason = self._board.audio.host_output_error or "host sounddevice is not available"
            self._audio_status.setText(f"Host audio unavailable: {reason}")

    def _create_sim_input_group(self) -> QGroupBox:
        group = QGroupBox("Sim keyboard/gamepad", self)
        layout = QVBoxLayout(group)

        self._key_checks: dict[KeyCode, QCheckBox] = {}
        for spec in DEFAULT_SIM_KEYS:
            check = QCheckBox(spec.title, group)
            check.stateChanged.connect(lambda _state, key=spec.key: self._apply_sim_key(key))
            self._key_checks[spec.key] = check
            layout.addWidget(check)

        return group

    def _create_display_group(self) -> QGroupBox:
        group = QGroupBox("Sim display", self)
        layout = QVBoxLayout(group)

        self._display_label = QLabel("display is not running", group)
        self._display_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._display_label.setFixedSize(320, 320)
        self._display_label.setStyleSheet("background: #202020; color: #b0b0b0; border: 1px solid #606060;")
        layout.addWidget(self._display_label)

        self._display_frame_label = QLabel("frame: -", group)
        layout.addWidget(self._display_frame_label)

        draw_button = QPushButton("Send: display-test", group)
        draw_button.clicked.connect(self._run_display_test)
        layout.addWidget(draw_button)
        return group

    def _create_i2c_group(self, *, oled: bool) -> QGroupBox:
        group = QGroupBox("I2C debug bus", self)
        layout = QVBoxLayout(group)

        self._oled_check = QCheckBox("SSD1306 OLED at 0x3C", group)
        self._oled_check.setChecked(oled)
        self._oled_check.stateChanged.connect(self._apply_i2c_devices)
        layout.addWidget(self._oled_check)

        address_row = QHBoxLayout()
        self._i2c_address_spin = HexSpinBox(group)
        address_row.addWidget(QLabel("Address", group))
        address_row.addWidget(self._i2c_address_spin)

        attach_button = QPushButton("Attach", group)
        attach_button.clicked.connect(self._attach_i2c_address)
        address_row.addWidget(attach_button)

        detach_button = QPushButton("Detach", group)
        detach_button.clicked.connect(self._detach_i2c_address)
        address_row.addWidget(detach_button)
        layout.addLayout(address_row)

        self._i2c_devices = QListWidget(group)
        layout.addWidget(self._i2c_devices)

        scan_button = QPushButton("Send: i2c-scan --quiet", group)
        scan_button.clicked.connect(self._run_i2c_scan)
        layout.addWidget(scan_button)

        self._refresh_i2c_list()
        return group

    def _browse_library(self) -> None:
        path, _ = QFileDialog.getOpenFileName(self, "Select LexxOS simulator library", str(Path.cwd()))
        if path:
            self._lib_path_edit.setText(path)

    def _sd_storage_mode(self) -> str:
        return str(self._sd_mode_combo.currentData() or "image")

    def _update_sd_storage_mode(self) -> None:
        if not hasattr(self, "_sd_path_edit"):
            return
        if self._sd_storage_mode() == "directory":
            self._sd_path_edit.setPlaceholderText("опционально: папка, импортируемая как FAT-содержимое /dev/sd0")
            self._sd_browse_button.setText("Browse folder…")
        else:
            self._sd_path_edit.setPlaceholderText("опционально: raw/FAT image")
            self._sd_browse_button.setText("Browse image…")
        if hasattr(self, "_capacity_combo"):
            self._capacity_combo.setEnabled(self._sd_storage_mode() == "directory")
            self._filesystem_combo.setEnabled(self._sd_storage_mode() == "directory")
        if hasattr(self, "_sd_writable_check"):
            self._sd_writable_check.setEnabled(self._sd_storage_mode() == "image")

    def _browse_sd_storage(self) -> None:
        if self._sd_storage_mode() == "directory":
            path = QFileDialog.getExistingDirectory(self, "Select host directory for SD contents", str(Path.cwd()))
        else:
            path, _ = QFileDialog.getOpenFileName(self, "Select SD image", str(Path.cwd()))
        if path:
            self._sd_path_edit.setText(path)
            self._sd_inserted_check.setChecked(True)

    def boot(self) -> None:
        """Start the configured simulator after the window is shown."""
        self._boot()

    def _boot(self) -> None:
        if self._sim is not None:
            return

        library_text = self._lib_path_edit.text().strip()
        if not library_text:
            QMessageBox.warning(self, "LexxOS Emulator", "Simulator library path is empty.")
            return
        library_path = Path(library_text).expanduser()
        if not library_path.exists():
            QMessageBox.warning(self, "LexxOS Emulator", f"Simulator library was not found:\n{library_path}")
            return

        sd_text = self._sd_path_edit.text().strip()
        sd_path = Path(sd_text).expanduser() if sd_text else None
        sd_image: Path | None = None
        sd_directory: Path | None = None
        if sd_path is not None:
            if self._sd_storage_mode() == "directory":
                if not sd_path.exists() or not sd_path.is_dir():
                    QMessageBox.warning(self, "LexxOS Emulator", f"SD directory was not found:\n{sd_path}")
                    return
                sd_directory = sd_path
            else:
                if not sd_path.exists() or not sd_path.is_file():
                    QMessageBox.warning(self, "LexxOS Emulator", f"SD image was not found:\n{sd_path}")
                    return
                sd_image = sd_path

        try:
            self._board = VirtualBoard.dev_pico(
                sd_image=sd_image,
                sd_inserted=self._sd_inserted_check.isChecked(),
                sd_directory=sd_directory,
                sd_capacity=str(self._capacity_combo.currentData()),
                sd_filesystem=str(self._filesystem_combo.currentData()),
                sd_writable=self._sd_writable_check.isChecked(),
                oled=self._oled_check.isChecked(),
                audio_mode=str(self._audio_mode_combo.currentData()),
                audio_output=str(self._audio_output_combo.currentData()),
                audio_wav_path=Path(self._audio_wav_path_edit.text().strip() or "lexxos-audio.wav").expanduser(),
            )
            self._sim = LexxOsSim(library_path, self._board)
        except (OSError, ValueError, RuntimeError) as exc:
            if self._board is not None:
                self._board.close()
                self._board = None
            QMessageBox.critical(self, "LexxOS SD error", str(exc))
            return

        self._apply_i2c_devices()
        for key in self._key_checks:
            self._apply_sim_key(key)
        self._sim.set_sd_inserted(self._sd_inserted_check.isChecked())
        self._sim.boot()
        self._booted = True
        self._last_display_frame = -1
        self._run_timer.start()
        self._terminal.setFocus()
        self._update_audio_status()
        self.statusBar().showMessage("running")
        if (self._audio_mode_combo.currentData() != "off"
                and self._audio_output_combo.currentData() in ("auto", "on")
                and not self._board.audio.host_output_available):
            self.statusBar().showMessage("running (audio unavailable: see Audio settings)")

        self._update_buttons()

    def _shutdown(self) -> None:
        if self._sim is None:
            return
        self._sim.request_shutdown()
        self._update_buttons()

    def _export_sd_snapshot(self) -> None:
        # Never read an image while the emulated filesystem may be writing.
        if self._sim is None or self._board is None or self._sim.is_running():
            QMessageBox.warning(self, "LexxOS Emulator", "Stop LexxOS before exporting an SD snapshot.")
            return
        image = self._board.block(0).image_path
        if image is None or not image.exists():
            QMessageBox.warning(self, "LexxOS Emulator", "No SD image is attached.")
            return
        path, ok = QInputDialog.getText(
            self, "Export SD snapshot", "New output directory (must not exist):")
        if not ok or not path.strip():
            return
        try:
            result = export_fat32_snapshot(image, Path(path).expanduser())
        except (OSError, ValueError) as exc:
            QMessageBox.critical(self, "SD export failed", str(exc))
            return
        QMessageBox.information(self, "SD snapshot exported", str(result))

    def _reset(self) -> None:
        self._close_simulator()
        self._terminal.clear()
        self._boot()

    def _close_simulator(self) -> None:
        self._run_timer.stop()
        if self._sim is not None:
            try:
                self._append_output(self._sim.read_all())
                self._refresh_display_snapshot()
                self._sim.close()
            except LexxOsSimError as exc:
                self._append_output(f"\n[emulator] close error: {exc}\n")
            finally:
                self._sim = None
                self._board = None
                self._booted = False
                self._last_display_frame = -1
                self._display_label.setText("display is not running")
                self._display_label.setPixmap(QPixmap())
                self._display_frame_label.setText("frame: -")
        self._update_audio_status()
        self.statusBar().showMessage("stopped")
        self._update_buttons()

    def _send_terminal_input(self, payload: bytes) -> None:
        """Forward raw input; echo and line editing belong to LexxOS."""
        if self._sim is None or not self._sim.is_running():
            return
        try:
            self._sim.write(payload)
        except LexxOsSimError as exc:
            self.statusBar().showMessage(f"terminal input error: {exc}")

    def _run_display_test(self) -> None:
        if self._sim is None:
            QMessageBox.information(self, "LexxOS Emulator", "Boot the simulator before running display-test.")
            return
        self._sim.write_line("display-test")
        self._append_output("display-test\n")

    def _run_i2c_scan(self) -> None:
        if self._sim is None:
            QMessageBox.information(self, "LexxOS Emulator", "Boot the simulator before running i2c-scan.")
            return
        self._sim.write_line("i2c-scan --quiet")
        self._append_output("i2c-scan --quiet\n")

    def _apply_sd_inserted(self) -> None:
        if self._sim is not None:
            self._sim.set_sd_inserted(self._sd_inserted_check.isChecked())

    def _apply_gpio_input(self, pin: int) -> None:
        if self._sim is None:
            return
        check = self._gpio_checks[pin]
        self._sim.gpio_set_input(pin, check.isChecked())

    def _apply_sim_key(self, key: KeyCode) -> None:
        if self._sim is None:
            return
        check = self._key_checks[key]
        self._sim.set_key(key, check.isChecked())

    def _attach_i2c_address(self) -> None:
        address = self._i2c_address_spin.value() & 0x7F
        self._i2c_addresses().add(address)
        self._apply_i2c_devices()
        self._refresh_i2c_list()

    def _detach_i2c_address(self) -> None:
        address = self._i2c_address_spin.value() & 0x7F
        self._i2c_addresses().discard(address)
        if address == 0x3C:
            self._oled_check.setChecked(False)
        self._apply_i2c_devices()
        self._refresh_i2c_list()

    def _i2c_addresses(self) -> set[int]:
        if not hasattr(self, "_manual_i2c_addresses"):
            self._manual_i2c_addresses: set[int] = set()
        return self._manual_i2c_addresses

    def _apply_i2c_devices(self) -> None:
        if self._board is None:
            self._refresh_i2c_list()
            return

        bus = self._board.i2c_bus(0)
        if self._oled_check.isChecked():
            bus.attach(0x3C)
        else:
            bus.detach(0x3C)

        for address in self._i2c_addresses():
            bus.attach(address)
        self._refresh_i2c_list()

    def _refresh_i2c_list(self) -> None:
        self._i2c_devices.clear()
        addresses: set[int] = set(self._i2c_addresses())
        if self._oled_check.isChecked():
            addresses.add(0x3C)
        if self._board is not None:
            addresses.update(self._board.i2c_bus(0).present_addresses)
        for address in sorted(addresses):
            self._i2c_devices.addItem(f"0x{address:02X}")

    def _tick(self) -> None:
        if self._sim is None:
            self._run_timer.stop()
            return

        try:
            if self._sim.is_running():
                self._sim.run_ms(10)
            self._append_output(self._sim.read_all())
            self._refresh_display_snapshot()
            self._update_audio_status()
            if (self._board is not None
                    and self._audio_mode_combo.currentData() != "off"
                    and self._audio_output_combo.currentData() in ("auto", "on")
                    and not self._board.audio.host_output_available):
                self.statusBar().showMessage("running (audio unavailable: see Audio settings)")
            if not self._sim.is_running() and self._booted:
                self._run_timer.stop()
                self.statusBar().showMessage("stopped")
                self._update_buttons()
        except (LexxOsSimError, OSError) as exc:
            self._run_timer.stop()
            self._append_output(f"\n[emulator] runtime error: {exc}\n")
            self.statusBar().showMessage("error")
            self._update_buttons()

    def _refresh_display_snapshot(self) -> None:
        if self._sim is None:
            return

        info = self._sim.display_info()
        if int(info.frame_counter) == self._last_display_frame:
            return

        rgba = self._sim.display_rgba8888()
        image = QImage(rgba, info.width, info.height, info.width * 4, QImage.Format.Format_RGBA8888)
        pixmap = QPixmap.fromImage(image.copy())
        self._display_label.setPixmap(pixmap)
        self._display_frame_label.setText(f"frame: {info.frame_counter} ({info.width}x{info.height})")
        self._last_display_frame = int(info.frame_counter)

    def _append_output(self, text: str) -> None:
        self._terminal.feed(text)

    def _update_buttons(self) -> None:
        running = self._sim is not None
        self._audio_mode_combo.setEnabled(not running)
        self._audio_output_combo.setEnabled(not running)
        self._update_audio_wav_controls()
        self._boot_action.setEnabled(not running)
        self._shutdown_action.setEnabled(running)
        self._reset_action.setEnabled(running or bool(self._lib_path_edit.text().strip()))
        self._scan_action.setEnabled(running)
