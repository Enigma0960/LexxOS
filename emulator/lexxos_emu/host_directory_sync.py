"""Explicit, conflict-checked sync between a FAT32 image and a host directory.

Called through the emulator's sync callback *after* VFS sync. A changed volume
is unmounted by LexxOS before apply(), and mounted again afterwards. No image
sector is replaced while FatFs has the volume mounted.
"""
from __future__ import annotations

import hashlib
import os
import shutil
import tempfile
import uuid
from dataclasses import dataclass
from pathlib import Path

from .fat32_export import export_fat32_snapshot
from .fat32_image import create_fat32_image_from_directory


@dataclass(frozen=True)
class FileState:
    kind: str  # "d" or "f"
    digest: str = ""


def tree_state(directory: Path) -> dict[str, FileState]:
    """Hash contents, not just mtimes. Directory entries include empty dirs."""
    result: dict[str, FileState] = {}

    def visit(parent: Path, prefix: str) -> None:
        seen: set[str] = set()
        for entry in sorted(parent.iterdir(), key=lambda p: (p.name.casefold(), p.name)):
            key = entry.name.casefold()
            if key in seen:
                raise ValueError(f"case-insensitive name collision in {parent}: {entry.name}")
            seen.add(key)
            if entry.is_symlink():
                raise ValueError(f"symlinks are not supported in the SD host directory: {entry}")
            relative = prefix + entry.name
            if entry.is_dir():
                result[relative] = FileState("d")
                visit(entry, relative + "/")
            elif entry.is_file():
                before = entry.stat()
                digest = hashlib.sha256()
                with entry.open("rb") as stream:
                    for block in iter(lambda: stream.read(1024 * 1024), b""):
                        digest.update(block)
                after = entry.stat()
                if (before.st_size, before.st_mtime_ns, before.st_ino) != (
                        after.st_size, after.st_mtime_ns, after.st_ino
                ):
                    raise OSError(f"host file changed while hashing: {entry}")
                result[relative] = FileState("f", digest.hexdigest())
            else:
                raise ValueError(f"unsupported host directory entry: {entry}")

    visit(directory, "")
    return result


def merge_states(
        base: dict[str, FileState],
        host: dict[str, FileState],
        guest: dict[str, FileState],
) -> tuple[dict[str, FileState], dict[str, str]]:
    """Three-way merge with deletion support and parent/type conflict checks.

    Returns desired state and a per-file source ('host' or 'guest').
    """
    result: dict[str, FileState] = {}
    sources: dict[str, str] = {}
    for name in sorted(base.keys() | host.keys() | guest.keys()):
        old, current, incoming = base.get(name), host.get(name), guest.get(name)
        host_changed = current != old
        guest_changed = incoming != old
        if host_changed and guest_changed and current != incoming:
            raise ValueError(f"host/guest conflict: {name}")
        chosen = incoming if guest_changed else current
        if chosen is not None:
            result[name] = chosen
            if chosen.kind == "f":
                sources[name] = "guest" if guest_changed else "host"

    for name in result:
        parent = name.rpartition("/")[0]
        while parent:
            if result.get(parent) != FileState("d"):
                raise ValueError(f"directory deleted or replaced while children changed: {parent}")
            parent = parent.rpartition("/")[0]
    return result, sources


class HostDirectorySync:
    """Owns baseline, checksum preflight, and transactionally applied updates."""

    def __init__(self, source: Path, image: Path, *, capacity: str):
        self.source = source.resolve()
        self.image = image.resolve()
        self.capacity = capacity
        self.baseline = tree_state(self.source)
        self.write_generation = 0
        self.synced_generation = 0
        self._pending: tuple[dict[str, FileState], dict[str, FileState], int] | None = None
        self._guest_snapshot: tempfile.TemporaryDirectory[str] | None = None

    def mark_written(self) -> None:
        self.write_generation += 1

    def cancel(self) -> None:
        if self._guest_snapshot is not None:
            self._guest_snapshot.cleanup()
            self._guest_snapshot = None
        self._pending = None

    def preflight(self) -> int:
        """Return 0 when no content changes, 1 when remount is necessary."""
        self.cancel()
        host = tree_state(self.source)
        guest = self.baseline
        gen = self.write_generation
        if gen != self.synced_generation:
            snapshot = tempfile.TemporaryDirectory(prefix="lexxos-guest-")
            try:
                guest_path = Path(snapshot.name) / "guest"
                export_fat32_snapshot(self.image, guest_path)
                guest = tree_state(guest_path)
            except BaseException:
                snapshot.cleanup()
                raise
            self._guest_snapshot = snapshot
        if host == guest:
            # Both sides already have identical logical contents (even if mtimes differ).
            self.baseline = host
            self.synced_generation = gen
            self.cancel()
            return 0
        # Reject conflicts before the expensive remount step.
        merge_states(self.baseline, host, guest)
        self._pending = (host, guest, gen)
        return 1

    def apply(self) -> None:
        """Call only while /mnt/sd is unmounted. Exceptions preserve the old image."""
        if self._pending is None:
            raise RuntimeError("host directory sync apply without preflight")
        host, guest, gen = self._pending
        if gen != self.write_generation or tree_state(self.source) != host:
            raise RuntimeError("host directory or FAT volume changed during sync; retry")
        desired, sources = merge_states(self.baseline, host, guest)
        guest_path = Path(self._guest_snapshot.name) / "guest" if self._guest_snapshot else None
        parent = self.source.parent
        staging = Path(tempfile.mkdtemp(prefix=".lexxos-sync-", dir=parent))
        new_image = self.image.with_name(self.image.name + ".sync-new-" + uuid.uuid4().hex)
        old_image = self.image.with_name(self.image.name + ".sync-old-" + uuid.uuid4().hex)
        backup_host = parent / (".lexxos-host-backup-" + uuid.uuid4().hex)
        merged = staging / "merged"
        merged.mkdir()
        host_replaced = False
        old_image_backed_up = False
        committed = False
        try:
            # Materialize the *final* state away from both live directory and FAT.
            for name in sorted(desired, key=lambda p: (p.count("/"), p)):
                if desired[name].kind == "d":
                    (merged / name).mkdir()
            for name, state in sorted(desired.items()):
                if state.kind != "f":
                    continue
                root = guest_path if sources[name] == "guest" else self.source
                if root is None:
                    raise RuntimeError(f"missing guest snapshot for {name}")
                shutil.copy2(root / name, merged / name)
            if tree_state(merged) != desired:
                raise RuntimeError("files changed during staging; retry sync")
            create_fat32_image_from_directory(merged, new_image, capacity=self.capacity)
            if tree_state(self.source) != host or self.write_generation != gen:
                raise RuntimeError("host directory or FAT volume changed during image build")

            # The directory needs replacing only when FAT made logical edits.
            if guest != self.baseline:
                os.replace(self.source, backup_host)
                try:
                    os.replace(merged, self.source)
                except BaseException:
                    os.replace(backup_host, self.source)
                    raise
                host_replaced = True

            try:
                os.replace(self.image, old_image)
                old_image_backed_up = True
                os.replace(new_image, self.image)
            except BaseException:
                if old_image_backed_up:
                    os.replace(old_image, self.image)
                    old_image_backed_up = False
                if host_replaced:
                    os.replace(self.source, merged)
                    os.replace(backup_host, self.source)
                    host_replaced = False
                raise

            committed = True
            self.baseline = desired
            self.synced_generation = gen
            self.cancel()
        finally:
            # Leave recoverable backup paths if a filesystem error prevented rollback.
            if not host_replaced and backup_host.exists():
                # Successful rollback must not remove user data; do not delete backups here.
                pass
            if committed:
                if old_image_backed_up and self.image.exists():
                    old_image.unlink(missing_ok=True)
                if host_replaced and backup_host.exists():
                    shutil.rmtree(backup_host)
            if not old_image_backed_up:
                new_image.unlink(missing_ok=True)
            if committed or not host_replaced:
                shutil.rmtree(staging, ignore_errors=True)
