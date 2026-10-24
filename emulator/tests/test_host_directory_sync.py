from pathlib import Path

import pytest

from lexxos_emu.fat32_export import export_fat32_snapshot
from lexxos_emu.fat32_image import create_fat32_image_from_directory
from lexxos_emu.host_directory_sync import HostDirectorySync, FileState, merge_states, tree_state


def test_three_way_merge_and_conflicts():
    base = {"data.txt": FileState("f", "old")}
    host = {"data.txt": FileState("f", "new")}
    assert merge_states(base, host, base)[0] == host
    guest = {"data.txt": FileState("f", "guest")}
    with pytest.raises(ValueError, match="conflict"):
        merge_states(base, host, guest)
    assert merge_states(base, base, guest)[0] == guest


def test_directory_deletion_vs_host_addition():
    base = {"docs": FileState("d")}
    host = {**base, "docs/new.txt": FileState("f", "x")}
    with pytest.raises(ValueError, match="directory deleted"):
        merge_states(base, host, {})


def test_hash_detects_same_size_changes_and_noop(tmp_path):
    host = tmp_path / "host"
    host.mkdir()
    path = host / "data.txt"
    path.write_text("abc")
    before = tree_state(host)
    stat = path.stat()
    path.write_text("xyz")
    import os
    os.utime(path, ns=(stat.st_atime_ns, stat.st_mtime_ns))
    assert tree_state(host) != before


def test_live_host_to_guest_and_checksum_skip(tmp_path):
    host = tmp_path / "host"
    host.mkdir()
    (host / "old.txt").write_text("old")
    image = tmp_path / "sd.img"
    create_fat32_image_from_directory(host, image, capacity="256M")
    mirror = HostDirectorySync(host, image, capacity="256M")
    assert mirror.preflight() == 0  # no remount or rebuild
    (host / "new.txt").write_text("new")
    assert mirror.preflight() == 1
    mirror.apply()
    assert mirror.preflight() == 0
    exported = tmp_path / "exported"
    export_fat32_snapshot(image, exported)
    assert (exported / "new.txt").read_text() == "new"
    mirror.cancel()


def test_live_guest_to_host_and_conflict(tmp_path):
    host = tmp_path / "host"
    host.mkdir()
    (host / "data.txt").write_text("base")
    image = tmp_path / "sd.img"
    create_fat32_image_from_directory(host, image, capacity="256M")
    mirror = HostDirectorySync(host, image, capacity="256M")

    guest_src = tmp_path / "guest"
    guest_src.mkdir()
    (guest_src / "data.txt").write_text("guest")
    (guest_src / "guest-new.txt").write_text("created")
    create_fat32_image_from_directory(guest_src, image, capacity="256M")
    mirror.mark_written()
    assert mirror.preflight() == 1
    mirror.apply()
    assert (host / "data.txt").read_text() == "guest"
    assert (host / "guest-new.txt").read_text() == "created"
    assert mirror.preflight() == 0

    (guest_src / "data.txt").write_text("guest2")
    create_fat32_image_from_directory(guest_src, image, capacity="256M")
    mirror.mark_written()
    (host / "data.txt").write_text("host2")
    with pytest.raises(ValueError, match="conflict"):
        mirror.preflight()
    assert (host / "data.txt").read_text() == "host2"
    mirror.cancel()


def test_guest_metadata_only_write_skips_remount(tmp_path):
    host = tmp_path / "host"
    host.mkdir()
    (host / "x.txt").write_text("x")
    img = tmp_path / "sd.img"
    create_fat32_image_from_directory(host, img, capacity="256M")
    mirror = HostDirectorySync(host, img, capacity="256M")
    mirror.mark_written()
    assert mirror.preflight() == 0
    assert mirror.synced_generation == 1


def test_guest_delete_and_nested_host_addition(tmp_path):
    host = tmp_path / "host"
    host.mkdir()
    (host / "remove.txt").write_text("obsolete")
    (host / "keep.txt").write_text("keep")
    image = tmp_path / "sd.img"
    create_fat32_image_from_directory(host, image, capacity="256M")
    mirror = HostDirectorySync(host, image, capacity="256M")
    guest = tmp_path / "guest"
    guest.mkdir()
    (guest / "keep.txt").write_text("keep")
    create_fat32_image_from_directory(guest, image, capacity="256M")
    mirror.mark_written()
    (host / "nested").mkdir()
    (host / "nested" / "Host Long Filename.txt").write_text("from host")
    assert mirror.preflight() == 1
    mirror.apply()
    assert not (host / "remove.txt").exists()
    assert (host / "nested" / "Host Long Filename.txt").read_text() == "from host"
    output = tmp_path / "output"
    export_fat32_snapshot(image, output)
    assert not (output / "remove.txt").exists()
    assert (output / "nested" / "Host Long Filename.txt").read_text() == "from host"


def test_preflight_rejects_host_race(tmp_path):
    host = tmp_path / "host"
    host.mkdir()
    image = tmp_path / "sd.img"
    create_fat32_image_from_directory(host, image, capacity="256M")
    mirror = HostDirectorySync(host, image, capacity="256M")
    (host / "new.txt").write_text("v1")
    assert mirror.preflight() == 1
    (host / "new.txt").write_text("v2")
    with pytest.raises(RuntimeError, match="changed during sync"):
        mirror.apply()
    assert (host / "new.txt").read_text() == "v2"
    mirror.cancel()


def test_failed_image_rebuild_preserves_host_and_image(tmp_path, monkeypatch):
    import lexxos_emu.host_directory_sync as sync_module

    host = tmp_path / "host"
    host.mkdir()
    (host / "original.txt").write_text("before")
    image = tmp_path / "disk.img"
    create_fat32_image_from_directory(host, image, capacity="256M")
    mirror = HostDirectorySync(host, image, capacity="256M")
    (host / "added.txt").write_text("new")
    assert mirror.preflight() == 1
    original_size = image.stat().st_size

    def fail_build(*_args, **_kwargs):
        raise OSError("injected build failure")

    monkeypatch.setattr(sync_module, "create_fat32_image_from_directory", fail_build)
    with pytest.raises(OSError, match="injected"):
        mirror.apply()
    assert (host / "original.txt").read_text() == "before"
    assert (host / "added.txt").read_text() == "new"
    assert image.stat().st_size == original_size
    mirror.cancel()
