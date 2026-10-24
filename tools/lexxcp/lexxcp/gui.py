"""PySide user interface for LexxCP."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import sys
from typing import Callable

from PySide6 import QtCore, QtWidgets
from PySide6.QtCore import Signal
from serial.tools import list_ports

from .serial_shell import LexxSerialClient, RemoteEntry, join_remote, parent_remote


@dataclass(slots=True)
class OperationResult:
    ok: bool
    message: str
    entries: list[RemoteEntry] | None = None
    cwd: str | None = None


class Worker(QtCore.QThread):
    finished_result = Signal(object)
    progress = Signal(int, int)

    def __init__(self, action: Callable[[Callable[[int, int], None]], OperationResult],
                 parent: QtCore.QObject | None = None):
        super().__init__(parent)
        self._action = action

    def run(self) -> None:
        try:
            result = self._action(lambda done, total: self.progress.emit(done, total))
        except Exception as exc:  # noqa: BLE001 - GUI boundary: display any transfer/open error.
            result = OperationResult(ok=False, message=str(exc))
        self.finished_result.emit(result)


class MainWindow(QtWidgets.QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("LexxCP — LexxOS UART File Manager")
        self.resize(1100, 680)

        self._client: LexxSerialClient | None = None
        self._remote_cwd = "/"
        self._worker: Worker | None = None

        self._build_ui()
        self.refresh_ports()
        self.refresh_local_root()
        self._set_connected(False)

    def closeEvent(self, event) -> None:  # type: ignore[override]
        if self._client is not None:
            self._client.close()
        super().closeEvent(event)

    def _build_ui(self) -> None:
        root = QtWidgets.QWidget(self)
        self.setCentralWidget(root)
        layout = QtWidgets.QVBoxLayout(root)

        connection = QtWidgets.QHBoxLayout()
        layout.addLayout(connection)

        connection.addWidget(QtWidgets.QLabel("Port:"))
        self.port_combo = QtWidgets.QComboBox()
        self.port_combo.setMinimumWidth(260)
        connection.addWidget(self.port_combo)

        self.refresh_ports_button = QtWidgets.QPushButton("Refresh ports")
        self.refresh_ports_button.clicked.connect(self.refresh_ports)
        connection.addWidget(self.refresh_ports_button)

        connection.addWidget(QtWidgets.QLabel("Baud:"))
        self.baud_spin = QtWidgets.QSpinBox()
        self.baud_spin.setRange(1200, 4_000_000)
        self.baud_spin.setValue(115200)
        self.baud_spin.setSingleStep(115200)
        connection.addWidget(self.baud_spin)

        self.connect_button = QtWidgets.QPushButton("Connect")
        self.connect_button.clicked.connect(self.toggle_connection)
        connection.addWidget(self.connect_button)
        connection.addStretch(1)

        self.status_label = QtWidgets.QLabel("Disconnected")
        connection.addWidget(self.status_label)

        splitter = QtWidgets.QSplitter(QtCore.Qt.Orientation.Horizontal)
        layout.addWidget(splitter, 1)

        local_panel = QtWidgets.QWidget()
        local_layout = QtWidgets.QVBoxLayout(local_panel)
        local_header = QtWidgets.QHBoxLayout()
        local_layout.addLayout(local_header)
        local_header.addWidget(QtWidgets.QLabel("Computer:"))
        self.local_path_edit = QtWidgets.QLineEdit()
        self.local_path_edit.returnPressed.connect(self.change_local_root_from_edit)
        local_header.addWidget(self.local_path_edit, 1)
        self.local_home_button = QtWidgets.QPushButton("Home")
        self.local_home_button.clicked.connect(self.go_local_home)
        local_header.addWidget(self.local_home_button)

        self.local_model = QtWidgets.QFileSystemModel(self)
        self.local_model.setRootPath(str(Path.home()))
        self.local_view = QtWidgets.QTreeView()
        self.local_view.setModel(self.local_model)
        self.local_view.setSelectionMode(QtWidgets.QAbstractItemView.SelectionMode.SingleSelection)
        self.local_view.setSortingEnabled(True)
        self.local_view.doubleClicked.connect(self.local_double_clicked)
        local_layout.addWidget(self.local_view, 1)
        splitter.addWidget(local_panel)

        middle_panel = QtWidgets.QWidget()
        middle_layout = QtWidgets.QVBoxLayout(middle_panel)
        middle_layout.addStretch(1)
        self.upload_button = QtWidgets.QPushButton("Upload →")
        self.upload_button.clicked.connect(self.upload_selected)
        middle_layout.addWidget(self.upload_button)
        self.download_button = QtWidgets.QPushButton("← Download")
        self.download_button.clicked.connect(self.download_selected)
        middle_layout.addWidget(self.download_button)
        middle_layout.addSpacing(24)
        self.progress = QtWidgets.QProgressBar()
        self.progress.setRange(0, 100)
        self.progress.setValue(0)
        middle_layout.addWidget(self.progress)
        middle_layout.addStretch(1)
        splitter.addWidget(middle_panel)

        remote_panel = QtWidgets.QWidget()
        remote_layout = QtWidgets.QVBoxLayout(remote_panel)
        remote_header = QtWidgets.QHBoxLayout()
        remote_layout.addLayout(remote_header)
        remote_header.addWidget(QtWidgets.QLabel("LexxOS:"))
        self.remote_path_edit = QtWidgets.QLineEdit("/")
        self.remote_path_edit.returnPressed.connect(self.change_remote_path_from_edit)
        remote_header.addWidget(self.remote_path_edit, 1)
        self.remote_up_button = QtWidgets.QPushButton("Up")
        self.remote_up_button.clicked.connect(self.remote_up)
        remote_header.addWidget(self.remote_up_button)
        self.remote_refresh_button = QtWidgets.QPushButton("Refresh")
        self.remote_refresh_button.clicked.connect(self.refresh_remote)
        remote_header.addWidget(self.remote_refresh_button)

        self.remote_view = QtWidgets.QTreeWidget()
        self.remote_view.setHeaderLabels(["Name", "Type", "Size"])
        self.remote_view.itemDoubleClicked.connect(self.remote_double_clicked)
        remote_layout.addWidget(self.remote_view, 1)
        splitter.addWidget(remote_panel)
        splitter.setSizes([480, 120, 480])

        self.log = QtWidgets.QPlainTextEdit()
        self.log.setReadOnly(True)
        self.log.setMaximumBlockCount(500)
        layout.addWidget(self.log, 0)

    def log_message(self, text: str) -> None:
        self.log.appendPlainText(text)
        self.status_label.setText(text)

    def refresh_ports(self) -> None:
        current = self.port_combo.currentData()
        self.port_combo.clear()
        for port in list_ports.comports():
            label = f"{port.device} — {port.description}"
            self.port_combo.addItem(label, port.device)
        if current is not None:
            index = self.port_combo.findData(current)
            if index >= 0:
                self.port_combo.setCurrentIndex(index)

    def toggle_connection(self) -> None:
        if self._client is not None:
            self._client.close()
            self._client = None
            self._set_connected(False)
            self.log_message("Disconnected")
            return

        port = self.port_combo.currentData()
        if not port:
            QtWidgets.QMessageBox.warning(self, "LexxCP", "Select a serial port first.")
            return

        baudrate = int(self.baud_spin.value())
        self._run_worker(lambda _progress: self._connect_worker(port, baudrate), self._connect_done)

    def _connect_worker(self, port: str, baudrate: int) -> OperationResult:
        client = LexxSerialClient(port, baudrate)
        client.serial.write(b"\r")
        cwd = client.wait_for_prompt(timeout_s=6.0)
        self._pending_client = client
        entries = client.list_dir(cwd)
        return OperationResult(ok=True, message=f"Connected to {port}", entries=entries, cwd=cwd)

    def _connect_done(self, result: OperationResult) -> None:
        if result.ok:
            self._client = getattr(self, "_pending_client", None)
            self._remote_cwd = result.cwd or "/"
            self._set_connected(True)
            self._show_remote_entries(result.entries or [], self._remote_cwd)
        else:
            pending = getattr(self, "_pending_client", None)
            if pending is not None:
                pending.close()
            self._client = None
            self._set_connected(False)
        self.log_message(result.message)

    def _set_connected(self, connected: bool) -> None:
        self.connect_button.setText("Disconnect" if connected else "Connect")
        for widget in (self.upload_button, self.download_button, self.remote_up_button, self.remote_refresh_button,
                       self.remote_path_edit):
            widget.setEnabled(connected)

    def refresh_local_root(self) -> None:
        path = str(Path.home())
        self.local_path_edit.setText(path)
        self.local_view.setRootIndex(self.local_model.index(path))

    def go_local_home(self) -> None:
        self.refresh_local_root()

    def change_local_root_from_edit(self) -> None:
        path = Path(self.local_path_edit.text()).expanduser()
        if not path.exists():
            QtWidgets.QMessageBox.warning(self, "LexxCP", "Local path does not exist.")
            return
        if path.is_file():
            path = path.parent
        self.local_path_edit.setText(str(path))
        self.local_view.setRootIndex(self.local_model.index(str(path)))

    def local_double_clicked(self, index: QtCore.QModelIndex) -> None:
        path = Path(self.local_model.filePath(index))
        if path.is_dir():
            self.local_path_edit.setText(str(path))
            self.local_view.setRootIndex(self.local_model.index(str(path)))

    def selected_local_path(self) -> Path | None:
        indexes = self.local_view.selectionModel().selectedIndexes()
        if not indexes:
            return None
        return Path(self.local_model.filePath(indexes[0]))

    def selected_remote_entry(self) -> RemoteEntry | None:
        items = self.remote_view.selectedItems()
        if not items:
            return None
        return items[0].data(0, QtCore.Qt.ItemDataRole.UserRole)

    def change_remote_path_from_edit(self) -> None:
        self.refresh_remote(self.remote_path_edit.text().strip() or "/")

    def refresh_remote(self, path: str | None = None) -> None:
        client = self._require_client()
        if client is None:
            return
        target = path or self._remote_cwd
        self._run_worker(lambda _progress: self._list_remote_worker(client, target), self._remote_list_done)

    def _list_remote_worker(self, client: LexxSerialClient, path: str) -> OperationResult:
        entries = client.list_dir(path)
        return OperationResult(ok=True, message=f"Listed {path}", entries=entries, cwd=path)

    def _remote_list_done(self, result: OperationResult) -> None:
        if result.ok:
            self._remote_cwd = result.cwd or self._remote_cwd
            self._show_remote_entries(result.entries or [], self._remote_cwd)
        self.log_message(result.message)

    def _show_remote_entries(self, entries: list[RemoteEntry], cwd: str) -> None:
        self.remote_path_edit.setText(cwd)
        self.remote_view.clear()
        for entry in entries:
            item = QtWidgets.QTreeWidgetItem([
                entry.name,
                "dir" if entry.is_dir else "file",
                "" if entry.size is None else str(entry.size),
            ])
            item.setData(0, QtCore.Qt.ItemDataRole.UserRole, entry)
            self.remote_view.addTopLevelItem(item)
        for column in range(3):
            self.remote_view.resizeColumnToContents(column)

    def remote_double_clicked(self, item: QtWidgets.QTreeWidgetItem) -> None:
        entry: RemoteEntry = item.data(0, QtCore.Qt.ItemDataRole.UserRole)
        if entry.is_dir:
            self.refresh_remote(entry.path)

    def remote_up(self) -> None:
        self.refresh_remote(parent_remote(self._remote_cwd))

    def upload_selected(self) -> None:
        client = self._require_client()
        local_path = self.selected_local_path()
        if client is None or local_path is None:
            return
        if not local_path.is_file():
            QtWidgets.QMessageBox.warning(self, "LexxCP", "Select a local file to upload.")
            return
        remote_path = join_remote(self._remote_cwd, local_path.name)
        self.progress.setValue(0)
        self._run_worker(lambda progress: self._upload_worker(client, local_path, remote_path, progress),
                         self._transfer_done)

    def _upload_worker(self, client: LexxSerialClient, local_path: Path, remote_path: str,
                       progress: Callable[[int, int], None]) -> OperationResult:
        client.upload(local_path, remote_path, progress)
        entries = client.list_dir(self._remote_cwd)
        return OperationResult(ok=True, message=f"Uploaded {local_path.name} to {remote_path}", entries=entries,
                               cwd=self._remote_cwd)

    def download_selected(self) -> None:
        client = self._require_client()
        entry = self.selected_remote_entry()
        if client is None or entry is None:
            return
        if entry.is_dir:
            QtWidgets.QMessageBox.warning(self, "LexxCP", "Select a LexxOS file to download.")
            return
        local_dir = Path(self.local_path_edit.text()).expanduser()
        if local_dir.is_file():
            local_dir = local_dir.parent
        local_path = local_dir / entry.name
        self.progress.setValue(0)
        self._run_worker(lambda progress: self._download_worker(client, entry.path, local_path, progress),
                         self._transfer_done)

    def _download_worker(self, client: LexxSerialClient, remote_path: str, local_path: Path,
                         progress: Callable[[int, int], None]) -> OperationResult:
        result_path = client.download(remote_path, local_path, progress)
        return OperationResult(ok=True, message=f"Downloaded {remote_path} to {result_path}")

    def _transfer_done(self, result: OperationResult) -> None:
        if result.entries is not None and result.cwd is not None:
            self._show_remote_entries(result.entries, result.cwd)
        self.progress.setValue(100 if result.ok else 0)
        self.log_message(result.message)

    def _require_client(self) -> LexxSerialClient | None:
        if self._client is None:
            QtWidgets.QMessageBox.warning(self, "LexxCP", "Connect to LexxOS first.")
            return None
        return self._client

    def _run_worker(self, action: Callable[[Callable[[int, int], None]], OperationResult],
                    done: Callable[[OperationResult], None]) -> None:
        if self._worker is not None:
            QtWidgets.QMessageBox.information(self, "LexxCP", "Another operation is already running.")
            return
        self._set_busy(True)
        worker = Worker(action, self)
        self._worker = worker
        worker.progress.connect(self._on_progress)
        worker.finished_result.connect(done)
        worker.finished_result.connect(lambda _result: self._worker_finished())
        worker.start()

    def _worker_finished(self) -> None:
        self._worker = None
        self._set_busy(False)

    def _set_busy(self, busy: bool) -> None:
        for widget in (
                self.refresh_ports_button,
                self.connect_button,
                self.upload_button,
                self.download_button,
                self.remote_up_button,
                self.remote_refresh_button,
        ):
            widget.setEnabled(not busy and (
                        widget not in {self.upload_button, self.download_button, self.remote_up_button,
                                       self.remote_refresh_button} or self._client is not None))

    def _on_progress(self, done: int, total: int) -> None:
        if total <= 0:
            self.progress.setValue(0)
            return
        self.progress.setValue(max(0, min(100, int(done * 100 / total))))


def main() -> int:
    app = QtWidgets.QApplication(sys.argv)
    window = MainWindow()
    window.show()
    return app.exec()
