from __future__ import annotations

import os
import re
import sys
import json
import shlex
import subprocess
from pathlib import Path

try:
    from PySide6.QtCore import QProcess, QProcessEnvironment, QSettings, Qt, QUrl
    from PySide6.QtGui import QDesktopServices, QGuiApplication
    from PySide6.QtWidgets import (
        QApplication,
        QCheckBox,
        QComboBox,
        QFileDialog,
        QGridLayout,
        QGroupBox,
        QHBoxLayout,
        QLabel,
        QLineEdit,
        QMainWindow,
        QMessageBox,
        QPushButton,
        QPlainTextEdit,
        QTabWidget,
        QVBoxLayout,
        QWidget,
    )
except ModuleNotFoundError as error:
    print(
        "lexxos-build-tool: PySide6 is not installed.\n"
        "Install it in the current Python environment, for example:\n"
        "  python3 -m pip install -r tools/build-tool/requirements.txt",
        file=sys.stderr,
    )
    raise SystemExit(1) from error

if __package__ in {None, ""}:
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from lexxos_build_tool.model import FEATURES, PLATFORMS, BoardOption, PlatformOption, default_path_text
else:
    from .model import FEATURES, PLATFORMS, BoardOption, PlatformOption, default_path_text

PICO_PICOTOOL_FETCH_PATH_DEFAULT = "${sourceDir}/build/tools/picotool"


class PathRow(QWidget):
    def __init__(self, placeholder: str = "") -> None:
        super().__init__()
        self.edit = QLineEdit()
        self.edit.setPlaceholderText(placeholder)
        self.button = QPushButton("...")
        self.button.setMaximumWidth(34)
        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.addWidget(self.edit)
        layout.addWidget(self.button)
        self.button.clicked.connect(self._browse)

    def text(self) -> str:
        return self.edit.text().strip()

    def setText(self, text: str) -> None:  # noqa: N802 - Qt-style API symmetry
        self.edit.setText(text)

    def _browse(self) -> None:
        current = self.text()
        start_dir = current if current else str(Path.home())
        path = QFileDialog.getExistingDirectory(self, "Select directory", start_dir)
        if path:
            self.setText(path)


class BuildToolWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self._root = self._repository_root()
        self._process: QProcess | None = None
        self._settings = QSettings("LexxOS", "BuildTool")
        self._platform: PlatformOption = PLATFORMS["pico"]
        self._settings_ready = False

        self.setWindowTitle("LexxOS Build Tool")
        self.resize(1220, 820)

        self._platform_combo = QComboBox()
        for key, platform in PLATFORMS.items():
            self._platform_combo.addItem(platform.label, key)
        saved_platform = self._settings_string("last/platform", "pico")
        saved_platform_index = self._platform_combo.findData(saved_platform)
        self._platform_combo.setCurrentIndex(
            saved_platform_index if saved_platform_index >= 0 else self._platform_combo.findData("pico"))

        self._board_combo = QComboBox()
        self._board_combo.setEditable(True)

        self._preset_combo = QComboBox()
        self._preset_combo.setEditable(True)

        self._build_dir = QLineEdit()
        self._user_preset_name = QLineEdit()
        self._user_preset_name.setPlaceholderText("CMakeUserPresets.json preset name, e.g. user-pico-gb")
        self._user_preset_name.setText("user-pico")

        self._cmake_bin = QLineEdit(os.environ.get("LEXXOS_CMAKE_BIN", "cmake"))
        self._ninja_bin = QLineEdit(os.environ.get("LEXXOS_NINJA_BIN", "ninja"))
        self._freertos_path = PathRow("third_party/FreeRTOS")
        self._freertos_path.setText(str(self._root / "third_party" / "FreeRTOS"))
        self._clion_path = PathRow("Optional path to CLion application or executable")
        self._debugger_path = PathRow("Optional debugger/OpenOCD/GDB path")

        self._pico_sdk_path = PathRow("~/.pico-sdk/sdk/2.2.0")
        self._pico_toolchain_path = PathRow("~/.pico-sdk/toolchain/14_2_Rel1")
        self._picotool_path = PathRow(PICO_PICOTOOL_FETCH_PATH_DEFAULT)
        self._picotool_path.setText(PICO_PICOTOOL_FETCH_PATH_DEFAULT)
        self._pico_board = QLineEdit("pico")
        self._pico_clock = QLineEdit()
        self._st7735_baud = QLineEdit()
        self._pico_board_label = QLabel("PICO_BOARD")

        self._arm_toolchain_path = PathRow("~/.pico-sdk/toolchain/14_2_Rel1")
        self._stm32_toolchain_file = QLineEdit("config/cmake/toolchains/arm-none-eabi.cmake")
        self._stm32_bringup_only = QCheckBox("Bring-up only")
        self._stm32_experimental_rp2040_sdio = QCheckBox("Enable experimental RP2040 SDIO skeleton")

        self._espidf_path = PathRow("~/.espressif/v6.0.1/esp-idf")
        env_idf_path = os.environ.get("LEXXOS_IDF_PATH") or os.environ.get("IDF_PATH")
        if env_idf_path:
            self._espidf_path.setText(env_idf_path)
        else:
            candidate = Path.home() / ".espressif" / "v6.0.1" / "esp-idf"
            self._espidf_path.setText(str(candidate))
        self._espidf_target = QLineEdit("esp32")
        self._espidf_auto_export = QCheckBox("Source ESP-IDF export.sh automatically for CLI checks")
        self._espidf_auto_export.setChecked(True)
        self._espidf_flash_port = QLineEdit()
        self._espidf_flash_baud = QLineEdit("921600")
        self._espidf_target_label = QLabel("IDF_TARGET")

        self._feature_checks: dict[str, QCheckBox] = {}
        for key, label, default in FEATURES:
            checkbox = QCheckBox(label)
            checkbox.setChecked(default == "ON")
            self._feature_checks[key] = checkbox

        self._custom_options = QPlainTextEdit()
        self._custom_options.setPlaceholderText(
            "One CMake -D option per line, for example:\nLEXXOS_SD_BACKEND=spi\nLEXXOS_PICO_SYS_CLOCK_KHZ=250000")
        self._custom_options.setMaximumHeight(110)

        self._instructions = QPlainTextEdit()
        self._instructions.setReadOnly(True)
        self._instructions.setMaximumHeight(150)

        self._command_preview = QPlainTextEdit()
        self._command_preview.setReadOnly(True)
        self._command_preview.setLineWrapMode(QPlainTextEdit.LineWrapMode.NoWrap)
        self._command_preview.setMaximumHeight(130)

        self._output = QPlainTextEdit()
        self._output.setReadOnly(True)
        self._output.setLineWrapMode(QPlainTextEdit.LineWrapMode.NoWrap)

        self._generate_button = QPushButton("Regenerate command")
        self._copy_button = QPushButton("Copy command")
        self._export_preset_button = QPushButton("Export CMakeUserPresets.json")
        self._open_user_presets_button = QPushButton("Open CMakeUserPresets.json")
        self._open_build_dir_button = QPushButton("Open build dir")
        self._reset_platform_button = QPushButton("Reset platform settings")
        self._configure_button = QPushButton("Run configure")
        self._check_button = QPushButton("Run preflight check")
        self._stop_button = QPushButton("Stop")
        self._clear_button = QPushButton("Clear log")
        self._stop_button.setEnabled(False)

        self._status = QLabel(f"Root: {self._root}")
        self._status.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)

        self._groups: dict[str, QGroupBox] = {}
        self._board_dependent_widgets: list[QWidget] = []
        self.setCentralWidget(self._create_layout())
        self._connect_signals()
        self._sync_platform()
        self._restore_window_geometry()
        self._settings_ready = True

    @staticmethod
    def _repository_root() -> Path:
        start_paths: list[Path] = []

        env_root = os.environ.get("LEXXOS_ROOT")
        if env_root:
            start_paths.append(Path(env_root).expanduser())

        start_paths.append(Path.cwd())
        if getattr(sys, "frozen", False):
            start_paths.append(Path(sys.executable).resolve())
            start_paths.append(Path(sys.argv[0]).resolve())
        start_paths.append(Path(__file__).resolve())

        checked: list[str] = []
        for start_path in start_paths:
            checked.append(str(start_path))
            root = BuildToolWindow._repository_root_from_start(start_path)
            if root is not None:
                return root

        checked_text = "\n  ".join(checked)
        raise RuntimeError(
            "repository root was not found.\n"
            "Set LEXXOS_ROOT to the LexxOS repository root or run the tool from the repository root.\n"
            "Checked:\n"
            f"  {checked_text}"
        )

    @staticmethod
    def _repository_root_from_start(start_path: Path) -> Path | None:
        path = start_path.parent if start_path.is_file() else start_path
        for candidate in [path.parent, *path.parents]:
            if (candidate / "CMakeLists.txt").is_file() and (candidate / "CMakePresets.json").is_file():
                return candidate
        return None

    def _create_layout(self) -> QWidget:
        root_widget = QWidget()
        root_layout = QVBoxLayout(root_widget)

        tabs = QTabWidget()
        tabs.addTab(self._create_configure_tab(), "Configurator")
        tabs.addTab(self._create_output_tab(), "Output")
        root_layout.addWidget(tabs, stretch=1)
        root_layout.addWidget(self._status)
        return root_widget

    def _create_configure_tab(self) -> QWidget:
        widget = QWidget()
        layout = QVBoxLayout(widget)

        target_group = QGroupBox("Platform and board")
        target_layout = QGridLayout(target_group)
        target_layout.addWidget(QLabel("Platform"), 0, 0)
        target_layout.addWidget(self._platform_combo, 0, 1)
        target_layout.addWidget(QLabel("Board"), 0, 2)
        target_layout.addWidget(self._board_combo, 0, 3)
        target_layout.addWidget(QLabel("Preset"), 1, 0)
        target_layout.addWidget(self._preset_combo, 1, 1)
        target_layout.addWidget(QLabel("Manual build dir"), 1, 2)
        target_layout.addWidget(self._build_dir, 1, 3)
        target_layout.addWidget(QLabel("User preset name"), 2, 0)
        target_layout.addWidget(self._user_preset_name, 2, 1, 1, 3)
        layout.addWidget(target_group)

        feature_group = self._feature_group()
        common_group = self._common_group()
        pico_group = self._pico_group()
        stm32_group = self._stm32_group()
        espidf_group = self._espidf_group()
        debug_group = self._debug_group()
        custom_group = self._custom_group()

        self._groups = {
            "features": feature_group,
            "common": common_group,
            "pico": pico_group,
            "stm32": stm32_group,
            "espidf": espidf_group,
            "debug": debug_group,
        }

        layout.addWidget(feature_group)
        layout.addWidget(common_group)
        layout.addWidget(pico_group)
        layout.addWidget(stm32_group)
        layout.addWidget(espidf_group)
        layout.addWidget(debug_group)
        layout.addWidget(custom_group)

        instruction_group = QGroupBox("IDE / environment instructions")
        instruction_layout = QVBoxLayout(instruction_group)
        instruction_layout.addWidget(self._instructions)
        layout.addWidget(instruction_group)

        command_group = QGroupBox("Generated CMake configure command")
        command_layout = QVBoxLayout(command_group)
        command_layout.addWidget(self._command_preview)
        button_layout = QHBoxLayout()
        button_layout.addWidget(self._generate_button)
        button_layout.addWidget(self._copy_button)
        button_layout.addWidget(self._export_preset_button)
        button_layout.addWidget(self._open_user_presets_button)
        button_layout.addWidget(self._open_build_dir_button)
        button_layout.addWidget(self._reset_platform_button)
        button_layout.addWidget(self._configure_button)
        button_layout.addWidget(self._check_button)
        button_layout.addWidget(self._stop_button)
        button_layout.addStretch(1)
        button_layout.addWidget(self._clear_button)
        command_layout.addLayout(button_layout)
        layout.addWidget(command_group)
        layout.addStretch(1)
        return widget

    def _create_output_tab(self) -> QWidget:
        widget = QWidget()
        layout = QVBoxLayout(widget)
        layout.addWidget(self._output)
        return widget

    def _feature_group(self) -> QGroupBox:
        group = QGroupBox("Components")
        layout = QGridLayout(group)
        for index, (_, _, _) in enumerate(FEATURES):
            checkbox = self._feature_checks[FEATURES[index][0]]
            layout.addWidget(checkbox, index // 3, index % 3)
        return group

    def _common_group(self) -> QGroupBox:
        group = QGroupBox("Common tools")
        layout = QGridLayout(group)
        layout.addWidget(QLabel("CMake executable"), 0, 0)
        layout.addWidget(self._cmake_bin, 0, 1)
        layout.addWidget(QLabel("Ninja executable"), 0, 2)
        layout.addWidget(self._ninja_bin, 0, 3)
        layout.addWidget(QLabel("FreeRTOS path"), 1, 0)
        layout.addWidget(self._freertos_path, 1, 1, 1, 3)
        return group

    def _pico_group(self) -> QGroupBox:
        group = QGroupBox("Pico / RP2040 settings")
        layout = QGridLayout(group)
        layout.addWidget(QLabel("Pico SDK"), 0, 0)
        layout.addWidget(self._pico_sdk_path, 0, 1, 1, 3)
        layout.addWidget(QLabel("Pico toolchain"), 1, 0)
        layout.addWidget(self._pico_toolchain_path, 1, 1, 1, 3)
        layout.addWidget(QLabel("picotool fetch path"), 2, 0)
        layout.addWidget(self._picotool_path, 2, 1, 1, 3)
        layout.addWidget(self._pico_board_label, 3, 0)
        layout.addWidget(self._pico_board, 3, 1)
        self._board_dependent_widgets.extend([self._pico_board_label, self._pico_board])
        layout.addWidget(QLabel("Clock kHz"), 3, 2)
        layout.addWidget(self._pico_clock, 3, 3)
        layout.addWidget(QLabel("ST7735 baud"), 4, 0)
        layout.addWidget(self._st7735_baud, 4, 1)
        return group

    def _stm32_group(self) -> QGroupBox:
        group = QGroupBox("STM32 settings")
        layout = QGridLayout(group)
        layout.addWidget(QLabel("Arm toolchain path"), 0, 0)
        layout.addWidget(self._arm_toolchain_path, 0, 1, 1, 3)
        layout.addWidget(QLabel("Toolchain file"), 1, 0)
        layout.addWidget(self._stm32_toolchain_file, 1, 1, 1, 3)
        layout.addWidget(self._stm32_bringup_only, 2, 1)
        layout.addWidget(self._stm32_experimental_rp2040_sdio, 2, 2, 1, 2)
        return group

    def _espidf_group(self) -> QGroupBox:
        group = QGroupBox("ESP-IDF settings")
        layout = QGridLayout(group)
        layout.addWidget(QLabel("ESP-IDF path"), 0, 0)
        layout.addWidget(self._espidf_path, 0, 1, 1, 3)
        layout.addWidget(self._espidf_target_label, 1, 0)
        layout.addWidget(self._espidf_target, 1, 1)
        self._board_dependent_widgets.extend([self._espidf_target_label, self._espidf_target])
        layout.addWidget(QLabel("Flash port"), 1, 2)
        layout.addWidget(self._espidf_flash_port, 1, 3)
        layout.addWidget(QLabel("Flash baud"), 2, 0)
        layout.addWidget(self._espidf_flash_baud, 2, 1)
        layout.addWidget(self._espidf_auto_export, 2, 2, 1, 2)
        return group

    def _debug_group(self) -> QGroupBox:
        group = QGroupBox("IDE and debugger paths")
        layout = QGridLayout(group)
        layout.addWidget(QLabel("IDE path"), 0, 0)
        layout.addWidget(self._clion_path, 0, 1, 1, 3)
        layout.addWidget(QLabel("Debugger/OpenOCD/GDB"), 1, 0)
        layout.addWidget(self._debugger_path, 1, 1, 1, 3)
        return group

    def _custom_group(self) -> QGroupBox:
        group = QGroupBox("Additional CMake options")
        layout = QVBoxLayout(group)
        layout.addWidget(self._custom_options)
        return group

    def _connect_signals(self) -> None:
        self._platform_combo.currentIndexChanged.connect(self._sync_platform)
        self._board_combo.currentIndexChanged.connect(self._sync_board_defaults)
        self._board_combo.lineEdit().textChanged.connect(self._sync_board_defaults)
        self._preset_combo.currentIndexChanged.connect(self._update_command_preview)
        self._build_dir.textChanged.connect(self._update_command_preview)
        self._custom_options.textChanged.connect(self._update_command_preview)
        for checkbox in self._feature_checks.values():
            checkbox.stateChanged.connect(self._update_command_preview)
        for widget in [
            self._cmake_bin,
            self._ninja_bin,
            self._pico_board,
            self._pico_clock,
            self._st7735_baud,
            self._stm32_toolchain_file,
            self._espidf_target,
            self._espidf_flash_port,
            self._espidf_flash_baud,
        ]:
            widget.textChanged.connect(self._update_command_preview)
        for row in [
            self._freertos_path,
            self._pico_sdk_path,
            self._pico_toolchain_path,
            self._picotool_path,
            self._arm_toolchain_path,
            self._espidf_path,
            self._clion_path,
            self._debugger_path,
        ]:
            row.edit.textChanged.connect(self._update_command_preview)
        self._stm32_bringup_only.stateChanged.connect(self._update_command_preview)
        self._stm32_experimental_rp2040_sdio.stateChanged.connect(self._update_command_preview)
        self._espidf_auto_export.stateChanged.connect(self._update_command_preview)
        self._generate_button.clicked.connect(self._update_command_preview)
        self._copy_button.clicked.connect(self._copy_command)
        self._export_preset_button.clicked.connect(self._export_user_preset)
        self._open_user_presets_button.clicked.connect(self._open_user_presets)
        self._open_build_dir_button.clicked.connect(self._open_build_dir)
        self._reset_platform_button.clicked.connect(self._reset_current_platform_settings)
        self._configure_button.clicked.connect(self._run_configure)
        self._check_button.clicked.connect(self._run_check)
        self._stop_button.clicked.connect(self._stop_process)
        self._clear_button.clicked.connect(self._output.clear)

    def _sync_platform(self) -> None:
        if self._settings_ready:
            self._save_platform_settings(self._platform.key)
        key = self._platform_combo.currentData()
        self._platform = PLATFORMS[str(key)]
        self._board_combo.clear()
        for board in self._platform.boards:
            self._board_combo.addItem(board.name, board.cmake_board)
        index = self._board_combo.findData(self._platform.default_board)
        self._board_combo.setCurrentIndex(index if index >= 0 else 0)
        self._preset_combo.clear()
        for preset in self._platform.presets:
            self._preset_combo.addItem(preset.label, preset.configure_preset)
        self._preset_combo.setCurrentIndex(0)
        self._build_dir.setText(self._platform.default_build_dir)
        self._user_preset_name.setText(f"user-{self._platform.key}-{self._platform.default_board}")
        self._apply_platform_feature_defaults()
        self._load_platform_settings(self._platform.key)
        self._sync_board_defaults()
        for name, group in self._groups.items():
            group.setVisible(name in self._platform.visible_groups)
        self._instructions.setPlainText(self._platform.instructions)
        self._settings.setValue("last/platform", self._platform.key)
        self._update_command_preview()

    def _apply_platform_feature_defaults(self) -> None:
        for key, checkbox in self._feature_checks.items():
            value = self._platform.default_cmake_options.get(key)
            if value is not None:
                checkbox.setChecked(value.upper() in {"ON", "1", "TRUE", "YES"})
            elif key == "LEXXOS_ENABLE_GAMEBOY":
                checkbox.setChecked(self._platform.key == "pico")
        if self._platform.key == "espidf":
            self._feature_checks["LEXXOS_ENABLE_GAMEBOY"].setChecked(False)
            self._feature_checks["LEXXOS_ENABLE_GAMEBOY"].setEnabled(False)
        else:
            self._feature_checks["LEXXOS_ENABLE_GAMEBOY"].setEnabled(True)

    def _sync_board_defaults(self) -> None:
        board = self._selected_board_option()
        if board is None or board.manual:
            self._set_board_dependent_fields_visible(True)
            self._update_command_preview()
            return

        if board.preset:
            self._set_combo_text(self._preset_combo, board.preset)
        if board.build_dir and self._should_replace_board_default_text(self._build_dir.text()):
            self._build_dir.setText(board.build_dir)
        if board.user_preset_name and self._should_replace_board_default_text(self._user_preset_name.text()):
            self._user_preset_name.setText(board.user_preset_name)

        for key, value in board.cmake_defaults.items():
            if key == "PICO_BOARD":
                self._pico_board.setText(value)
            elif key == "IDF_TARGET":
                self._espidf_target.setText(value)

        for key, value in board.feature_defaults.items():
            self._set_feature_checked(key, value.upper() in {"ON", "1", "TRUE", "YES"})

        self._set_board_dependent_fields_visible(False)
        self._update_command_preview()

    def _should_replace_board_default_text(self, text: str) -> bool:
        value = text.strip()
        if not value:
            return True
        if value.startswith("user-"):
            return True
        return value in {platform.default_build_dir for platform in PLATFORMS.values()}

    def _set_board_dependent_fields_visible(self, visible: bool) -> None:
        for widget in self._board_dependent_widgets:
            widget.setVisible(visible)

    def _set_feature_checked(self, key: str, checked: bool) -> None:
        checkbox = self._feature_checks.get(key)
        if checkbox is not None:
            checkbox.setChecked(checked)

    def _load_platform_settings(self, platform_key: str) -> None:
        self._settings.beginGroup(f"platforms/{platform_key}")
        try:
            if not self._settings.contains("saved"):
                return

            self._set_combo_text(self._board_combo, self._settings_string("board", self._board_combo.currentText()))
            self._set_combo_text(self._preset_combo, self._settings_string("preset", self._preset_combo.currentText()))
            self._build_dir.setText(self._settings_string("build_dir", self._build_dir.text()))
            self._user_preset_name.setText(self._settings_string("user_preset_name", self._user_preset_name.text()))

            self._cmake_bin.setText(self._settings_string("cmake_bin", self._cmake_bin.text()))
            self._ninja_bin.setText(self._settings_string("ninja_bin", self._ninja_bin.text()))
            self._freertos_path.setText(self._settings_string("freertos_path", self._freertos_path.text()))
            self._clion_path.setText(self._settings_string("clion_path", self._clion_path.text()))
            self._debugger_path.setText(self._settings_string("debugger_path", self._debugger_path.text()))
            self._custom_options.setPlainText(
                self._settings_string("custom_options", self._custom_options.toPlainText()))

            self._pico_sdk_path.setText(self._settings_string("pico/sdk_path", self._pico_sdk_path.text()))
            self._pico_toolchain_path.setText(
                self._settings_string("pico/toolchain_path", self._pico_toolchain_path.text()))
            self._picotool_path.setText(self._settings_string("pico/picotool_path", self._picotool_path.text()))
            self._pico_board.setText(self._settings_string("pico/pico_board", self._pico_board.text()))
            self._pico_clock.setText(self._settings_string("pico/clock_khz", self._pico_clock.text()))
            self._st7735_baud.setText(self._settings_string("pico/st7735_baud", self._st7735_baud.text()))

            self._arm_toolchain_path.setText(
                self._settings_string("stm32/arm_toolchain_path", self._arm_toolchain_path.text()))
            self._stm32_toolchain_file.setText(
                self._settings_string("stm32/toolchain_file", self._stm32_toolchain_file.text()))
            self._stm32_bringup_only.setChecked(
                self._settings_bool("stm32/bringup_only", self._stm32_bringup_only.isChecked()))
            self._stm32_experimental_rp2040_sdio.setChecked(
                self._settings_bool("stm32/experimental_rp2040_sdio", self._stm32_experimental_rp2040_sdio.isChecked()))

            self._espidf_path.setText(self._settings_string("espidf/path", self._espidf_path.text()))
            self._espidf_target.setText(self._settings_string("espidf/target", self._espidf_target.text()))
            self._espidf_flash_port.setText(self._settings_string("espidf/flash_port", self._espidf_flash_port.text()))
            self._espidf_flash_baud.setText(self._settings_string("espidf/flash_baud", self._espidf_flash_baud.text()))
            self._espidf_auto_export.setChecked(
                self._settings_bool("espidf/auto_export", self._espidf_auto_export.isChecked()))

            for key, checkbox in self._feature_checks.items():
                checkbox.setChecked(self._settings_bool(f"features/{key}", checkbox.isChecked()))
        finally:
            self._settings.endGroup()

    def _save_platform_settings(self, platform_key: str) -> None:
        self._settings.beginGroup(f"platforms/{platform_key}")
        try:
            self._settings.setValue("saved", True)
            self._settings.setValue("board", self._board_combo.currentText().strip())
            self._settings.setValue("preset", self._preset_combo.currentText().strip())
            self._settings.setValue("build_dir", self._build_dir.text().strip())
            self._settings.setValue("user_preset_name", self._user_preset_name.text().strip())

            self._settings.setValue("cmake_bin", self._cmake_bin.text().strip())
            self._settings.setValue("ninja_bin", self._ninja_bin.text().strip())
            self._settings.setValue("freertos_path", self._freertos_path.text())
            self._settings.setValue("clion_path", self._clion_path.text())
            self._settings.setValue("debugger_path", self._debugger_path.text())
            self._settings.setValue("custom_options", self._custom_options.toPlainText())

            self._settings.setValue("pico/sdk_path", self._pico_sdk_path.text())
            self._settings.setValue("pico/toolchain_path", self._pico_toolchain_path.text())
            self._settings.setValue("pico/picotool_path", self._picotool_path.text())
            self._settings.setValue("pico/pico_board", self._pico_board.text().strip())
            self._settings.setValue("pico/clock_khz", self._pico_clock.text().strip())
            self._settings.setValue("pico/st7735_baud", self._st7735_baud.text().strip())

            self._settings.setValue("stm32/arm_toolchain_path", self._arm_toolchain_path.text())
            self._settings.setValue("stm32/toolchain_file", self._stm32_toolchain_file.text().strip())
            self._settings.setValue("stm32/bringup_only", self._stm32_bringup_only.isChecked())
            self._settings.setValue("stm32/experimental_rp2040_sdio", self._stm32_experimental_rp2040_sdio.isChecked())

            self._settings.setValue("espidf/path", self._espidf_path.text())
            self._settings.setValue("espidf/target", self._espidf_target.text().strip())
            self._settings.setValue("espidf/flash_port", self._espidf_flash_port.text().strip())
            self._settings.setValue("espidf/flash_baud", self._espidf_flash_baud.text().strip())
            self._settings.setValue("espidf/auto_export", self._espidf_auto_export.isChecked())

            for key, checkbox in self._feature_checks.items():
                self._settings.setValue(f"features/{key}", checkbox.isChecked())
        finally:
            self._settings.endGroup()
        self._settings.setValue("last/platform", platform_key)
        self._settings.sync()

    def _restore_window_geometry(self) -> None:
        geometry = self._settings.value("window/geometry")
        if geometry is not None:
            self.restoreGeometry(geometry)

    def closeEvent(self, event) -> None:  # noqa: N802 - Qt override
        if self._settings_ready:
            self._save_platform_settings(self._platform.key)
        self._settings.setValue("window/geometry", self.saveGeometry())
        self._settings.sync()
        super().closeEvent(event)

    def _settings_string(self, key: str, default: str = "") -> str:
        value = self._settings.value(key, default)
        if value is None:
            return default
        return str(value)

    def _settings_bool(self, key: str, default: bool) -> bool:
        value = self._settings.value(key, default)
        if isinstance(value, bool):
            return value
        if value is None:
            return default
        text = str(value).strip().lower()
        if text in {"1", "true", "yes", "on"}:
            return True
        if text in {"0", "false", "no", "off"}:
            return False
        return default

    @staticmethod
    def _set_combo_text(combo: QComboBox, value: str) -> None:
        index = combo.findText(value)
        if index >= 0:
            combo.setCurrentIndex(index)
        elif combo.isEditable():
            combo.setEditText(value)

    def _selected_preset(self) -> str:
        text = self._preset_combo.currentText().strip()
        index = self._preset_combo.currentIndex()
        if index >= 0 and text == self._preset_combo.itemText(index):
            return str(self._preset_combo.itemData(index) or "")
        return text

    def _selected_board(self) -> str:
        text = self._board_combo.currentText().strip()
        index = self._board_combo.currentIndex()
        if index >= 0 and text == self._board_combo.itemText(index):
            data = self._board_combo.itemData(index)
            if data is not None:
                return str(data)
        return text

    def _selected_board_option(self) -> BoardOption | None:
        index = self._board_combo.currentIndex()
        if 0 <= index < len(self._platform.boards):
            return self._platform.boards[index]
        selected_board = self._selected_board()
        for board in self._platform.boards:
            if board.cmake_board == selected_board or board.name == selected_board:
                return board
        return None

    def _selected_preset_option_label(self) -> str:
        return self._preset_combo.currentText().strip()

    def _cmake_definitions(self) -> dict[str, str]:
        platform = self._platform
        definitions = dict(platform.default_cmake_options)
        board = self._selected_board_option()
        definitions["LEXXOS_PLATFORM"] = platform.cmake_platform

        if board is not None and not board.manual:
            definitions.update(board.cmake_defaults)
        selected_board = self._selected_board()
        if selected_board:
            definitions["LEXXOS_BOARD"] = selected_board

        for key, checkbox in self._feature_checks.items():
            if checkbox.isEnabled():
                definitions[key] = "ON" if checkbox.isChecked() else "OFF"

        freertos = self._freertos_path.text()
        if freertos:
            definitions["FREERTOS_KERNEL_PATH"] = freertos

        if platform.key == "pico":
            self._add_if_set(definitions, "PICO_SDK_PATH", self._pico_sdk_path.text())
            self._add_if_set(definitions, "PICO_TOOLCHAIN_PATH", self._pico_toolchain_path.text())
            self._add_if_set(definitions, "PICOTOOL_FETCH_FROM_GIT_PATH",
                             self._cmake_source_dir_path(self._picotool_path.text()))
            if board is None or board.manual:
                self._add_if_set(definitions, "PICO_BOARD", self._pico_board.text().strip())

            self._add_if_set(definitions, "LEXXOS_PICO_SYS_CLOCK_KHZ", self._pico_clock.text().strip())
            self._add_if_set(definitions, "LEXXOS_ST7735_SPI_BAUDRATE", self._st7735_baud.text().strip())
        elif platform.key == "stm32f4":
            self._add_if_set(definitions, "LEXXOS_ARM_TOOLCHAIN_PATH", self._arm_toolchain_path.text())
            self._add_if_set(definitions, "CMAKE_TOOLCHAIN_FILE", self._stm32_toolchain_file.text().strip())
            definitions["LEXXOS_STM32F4_BRINGUP_ONLY"] = "ON" if self._stm32_bringup_only.isChecked() else "OFF"
            if self._stm32_experimental_rp2040_sdio.isChecked():
                definitions["LEXXOS_ENABLE_EXPERIMENTAL_RP2040_SDIO"] = "ON"
        elif platform.key == "espidf":
            if board is None or board.manual:
                self._add_if_set(definitions, "IDF_TARGET", self._espidf_target.text().strip())

            definitions["ESP_PLATFORM"] = "1"
            definitions["PYTHON_DEPS_CHECKED"] = "1"
            if self._espidf_path.text():
                definitions["LEXXOS_IDF_PATH"] = self._espidf_path.text()

        for line in self._custom_options.toPlainText().splitlines():
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            if line.startswith("-D"):
                line = line[2:]
            if "=" in line:
                key, value = line.split("=", 1)
                definitions[key.strip()] = value.strip()
        return definitions

    @staticmethod
    def _add_if_set(definitions: dict[str, str], key: str, value: str) -> None:
        if value:
            definitions[key] = value

    @staticmethod
    def _cmake_source_dir_path(value: str) -> str:
        text = value.strip()
        if not text or text.startswith("${"):
            return text
        if Path(text).expanduser().is_absolute():
            return text
        return "${sourceDir}/" + text.lstrip("/")

    def _command_line_cache_value(self, value: str) -> str:
        return value.replace("${sourceDir}", str(self._root))

    def _generated_command(self) -> list[str]:
        preset = self._selected_preset()
        cmake = self._cmake_bin.text().strip() or "cmake"
        if preset:
            command = [str(self._root / "tools" / "lexxos-cmake"), "--preset", preset]
            return command

        build_dir = self._build_dir.text().strip() or self._platform.default_build_dir
        command = [cmake, "-S", str(self._root), "-B", str(self._root / build_dir), "-G", "Ninja"]
        for key, value in sorted(self._cmake_definitions().items()):
            command.append(f"-D{key}={self._command_line_cache_value(value)}")
        return command

    def _check_command(self) -> list[str]:
        tool = str(self._root / "tools" / "lexxos-build")
        target_map = {
            "host": "host",
            "sim": "sim",
            "pico": "pico",
            "stm32f4": "stm32",
            "espidf": "esp32",
        }
        command = [tool, "check", target_map[self._platform.key]]
        if self._platform.key == "espidf" and self._espidf_path.text():
            command.extend(["--idf-path", self._espidf_path.text()])
            if not self._espidf_auto_export.isChecked():
                command.append("--no-espidf-export")
        return command

    def _update_command_preview(self) -> None:
        command = self._generated_command()
        self._command_preview.setPlainText(" ".join(shlex.quote(part) for part in command))
        self._update_instruction_suffix()

    def _update_instruction_suffix(self) -> None:
        notes = [self._platform.instructions]
        if self._platform.key == "pico":
            notes.append(
                "\nCLion: select a Pico profile or add a custom CMake profile with the generated command values.")
        elif self._platform.key == "stm32f4":
            notes.append("\nCLion: configure OpenOCD/ST-Link and use the ELF from the selected build directory.")
        elif self._platform.key == "espidf":
            notes.append(
                "\nCLion/ESP-IDF: verify IDF_PATH, serial port and flash target before using run/debug configurations.")
        self._instructions.setPlainText("\n".join(notes))

    def _copy_command(self) -> None:
        QGuiApplication.clipboard().setText(self._command_preview.toPlainText())
        self._status.setText("Command copied to clipboard")

    def _open_user_presets(self) -> None:
        path = self._root / "CMakeUserPresets.json"
        if not path.is_file():
            QMessageBox.information(
                self,
                "LexxOS Build Tool",
                "CMakeUserPresets.json does not exist yet. Export a user preset first.",
            )
            return
        self._open_path(path)

    def _open_build_dir(self) -> None:
        path = self._expanded_project_path(self._build_dir.text().strip() or self._platform.default_build_dir)
        if not path.exists():
            QMessageBox.information(
                self,
                "LexxOS Build Tool",
                f"Build directory does not exist yet:\n{path}\n\nRun configure first.",
            )
            return
        self._open_path(path)

    def _reset_current_platform_settings(self) -> None:
        platform_key = self._platform.key
        reply = QMessageBox.question(
            self,
            "LexxOS Build Tool",
            f"Reset saved settings for {self._platform.label}?\n\n"
            "Only this platform will be reset. Other platform settings are preserved.",
        )
        if reply != QMessageBox.StandardButton.Yes:
            return

        self._settings.remove(f"platforms/{platform_key}")
        self._settings.sync()
        was_ready = self._settings_ready
        self._settings_ready = False
        try:
            self._sync_platform()
        finally:
            self._settings_ready = was_ready
        self._append_line(f"[settings] reset platform: {platform_key}")
        self._status.setText(f"Reset settings for {self._platform.label}")

    def _open_path(self, path: Path) -> None:
        QDesktopServices.openUrl(QUrl.fromLocalFile(str(path)))

    def _export_user_preset(self) -> None:
        preset_name = self._sanitize_preset_name(self._user_preset_name.text())
        if not preset_name:
            QMessageBox.warning(self, "LexxOS Build Tool", "User preset name is empty.")
            return

        user_presets_path = self._root / "CMakeUserPresets.json"
        try:
            document = self._load_user_presets(user_presets_path)
        except ValueError as error:
            QMessageBox.warning(self, "LexxOS Build Tool", str(error))
            return

        configure_preset = self._user_configure_preset(preset_name)
        build_preset = self._user_build_preset(preset_name)

        self._replace_named_entry(document, "configurePresets", configure_preset)
        self._replace_named_entry(document, "buildPresets", build_preset)

        user_presets_path.write_text(json.dumps(document, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        self._append_line(f"[preset] wrote {user_presets_path}")
        self._append_line(f"[preset] configure: {preset_name}")
        self._append_line(f"[preset] build:     {preset_name}-build")
        self._status.setText(f"Exported {preset_name} to CMakeUserPresets.json")
        QMessageBox.information(
            self,
            "LexxOS Build Tool",
            "Preset exported to CMakeUserPresets.json.\n\n"
            "Reload the CMake project in CLion and select the new preset in the CMake profile list.",
        )

    def _load_user_presets(self, path: Path) -> dict[str, object]:
        if not path.exists():
            return {
                "version": 6,
                "cmakeMinimumRequired": {
                    "major": 3,
                    "minor": 20,
                    "patch": 0,
                },
                "configurePresets": [],
                "buildPresets": [],
            }

        try:
            document = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as error:
            raise ValueError(f"Invalid CMakeUserPresets.json: {error}") from error

        if not isinstance(document, dict):
            raise ValueError("Invalid CMakeUserPresets.json: root JSON value must be an object")
        document.setdefault("version", 6)
        document.setdefault("configurePresets", [])
        document.setdefault("buildPresets", [])
        return document

    def _user_configure_preset(self, preset_name: str) -> dict[str, object]:
        selected_preset = self._selected_preset()
        platform_name = self._platform.label
        board_name = self._selected_board()
        build_dir = self._preset_binary_dir()

        definitions = dict(self._cmake_definitions())
        environment: dict[str, str] = {}
        idf_path = definitions.get("LEXXOS_IDF_PATH", "")
        if self._platform.key == "espidf" and idf_path:
            environment["IDF_PATH"] = idf_path
            environment["LEXXOS_IDF_PATH"] = idf_path

            flash_port = self._espidf_flash_port.text().strip()
            flash_baud = self._espidf_flash_baud.text().strip()
            if flash_port:
                environment["ESPPORT"] = flash_port
            if flash_baud:
                environment["ESPBAUD"] = flash_baud

        preset: dict[str, object] = {
            "name": preset_name,
            "displayName": f"LexxOS {platform_name} ({board_name})",
            "description": "Generated by tools/build-tool. Local user preset, do not commit.",
            "binaryDir": build_dir,
            "cacheVariables": dict(sorted(definitions.items())),
        }
        if selected_preset:
            preset["inherits"] = selected_preset
        else:
            preset["generator"] = "Ninja"
        if environment:
            preset["environment"] = environment
        return preset

    def _user_build_preset(self, preset_name: str) -> dict[str, object]:
        return {
            "name": f"{preset_name}-build",
            "displayName": f"Build {preset_name}",
            "configurePreset": preset_name,
        }

    def _preset_binary_dir(self) -> str:
        build_dir = self._build_dir.text().strip() or self._platform.default_build_dir
        path = Path(build_dir).expanduser()
        if path.is_absolute():
            return str(path)
        return "${sourceDir}/" + build_dir.lstrip("/")

    def _expanded_project_path(self, value: str) -> Path:
        text = value.strip().replace("${sourceDir}", str(self._root))
        if not text:
            return self._root
        path = Path(text).expanduser()
        if path.is_absolute():
            return path
        return self._root / path

    @staticmethod
    def _replace_named_entry(document: dict[str, object], key: str, value: dict[str, object]) -> None:
        entries = document.setdefault(key, [])
        if not isinstance(entries, list):
            raise ValueError(f"Invalid CMakeUserPresets.json: {key} must be an array")
        name = value["name"]
        for index, entry in enumerate(entries):
            if isinstance(entry, dict) and entry.get("name") == name:
                entries[index] = value
                return
        entries.append(value)

    @staticmethod
    def _sanitize_preset_name(name: str) -> str:
        normalized = re.sub(r"[^A-Za-z0-9_.-]+", "-", name.strip())
        return normalized.strip("-_.")

    def _generated_process_environment(self) -> dict[str, str]:
        if self._platform.key != "espidf":
            return {}
        idf_path = self._espidf_path.text().strip()
        if not idf_path:
            return {}
        environment = {"IDF_PATH": idf_path, "LEXXOS_IDF_PATH": idf_path}
        flash_port = self._espidf_flash_port.text().strip()
        flash_baud = self._espidf_flash_baud.text().strip()
        if flash_port:
            environment["ESPPORT"] = flash_port
        if flash_baud:
            environment["ESPBAUD"] = flash_baud
        return environment

    def _run_configure(self) -> None:
        self._run_process(self._generated_command(), self._generated_process_environment())

    def _run_check(self) -> None:
        self._run_process(self._check_command())

    def _process_environment(self) -> QProcessEnvironment:
        environment = QProcessEnvironment.systemEnvironment()
        if self._platform.key == "espidf":
            for key, value in self._generated_process_environment().items():
                environment.insert(key, value)

        return environment

    def _run_process(self, command: list[str], environment: dict[str, str] | None = None) -> None:
        if self._process is not None:
            QMessageBox.warning(self, "LexxOS Build Tool", "A command is already running.")
            return
        self._append_line("$ " + " ".join(shlex.quote(part) for part in command))
        process = QProcess(self)
        process.setWorkingDirectory(str(self._root))

        process.setProcessChannelMode(QProcess.ProcessChannelMode.MergedChannels)
        process_environment = self._process_environment()

        if environment:
            for key, value in environment.items():
                if value:
                    process_environment.insert(key, value)

        process.setProcessEnvironment(process_environment)
        process.readyReadStandardOutput.connect(self._read_process_output)
        process.finished.connect(self._process_finished)
        self._process = process
        self._set_running(True)
        process.start(command[0], command[1:])

    def _read_process_output(self) -> None:
        if self._process is None:
            return
        data = bytes(self._process.readAllStandardOutput()).decode(errors="replace")
        if data:
            self._output.moveCursor(self._output.textCursor().MoveOperation.End)
            self._output.insertPlainText(data)
            self._output.moveCursor(self._output.textCursor().MoveOperation.End)

    def _process_finished(self, exit_code: int, exit_status: QProcess.ExitStatus) -> None:
        self._read_process_output()
        if exit_status == QProcess.ExitStatus.NormalExit:
            self._append_line(f"\n[finished] exit code: {exit_code}")
        else:
            self._append_line("\n[finished] process crashed or was terminated")
        self._process = None
        self._set_running(False)

    def _stop_process(self) -> None:
        if self._process is None:
            return
        self._process.terminate()
        if not self._process.waitForFinished(1500):
            self._process.kill()

    def _set_running(self, running: bool) -> None:
        self._configure_button.setEnabled(not running)
        self._check_button.setEnabled(not running)
        self._export_preset_button.setEnabled(not running)
        self._stop_button.setEnabled(running)
        self._status.setText("Running..." if running else f"Root: {self._root}")

    def _append_line(self, text: str) -> None:
        self._output.appendPlainText(text)


def main() -> int:
    app = QApplication(sys.argv)
    window = BuildToolWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
