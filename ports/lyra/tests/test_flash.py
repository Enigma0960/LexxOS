"""Exercise full-install safety and ordering with mocked devices, never sudo."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).parents[1]))
spec = importlib.util.spec_from_file_location("lyra_full_flash", Path(__file__).parents[1] / "flash.py")
flash = importlib.util.module_from_spec(spec)
spec.loader.exec_module(flash)


class MediaChecks(unittest.TestCase):
    def check(self, **changes):
        info = dict(DeviceIdentifier="disk9", WholeDisk=True, RemovableMedia=True,
                    Writable=True, DeviceBlockSize=512, Size=128 * 1024 ** 2,
                    MediaName="SD", DeviceTreePath="reader", BusProtocol="Secure Digital")
        info.update(changes)
        with patch.object(flash, "physical_disks", return_value=["/dev/disk9"]), \
                patch.object(flash.disk_tools, "disk_info", side_effect=[info, {"ParentWholeDisk": "disk0"}]):
            return flash.check_disk("/dev/disk9")

    def test_blank_card_needs_no_gpt(self):
        self.assertEqual(self.check()["raw"], "/dev/rdisk9")

    def test_nonremovable_readonly_partition_and_wrong_sector_rejected(self):
        for change in [dict(RemovableMedia=False), dict(Writable=False),
                       dict(WholeDisk=False), dict(DeviceBlockSize=4096), dict(Size=0)]:
            with self.subTest(change=change), self.assertRaises((flash.UnsafeTarget, ValueError)):
                self.check(**change)

    def test_system_and_partition_names_rejected(self):
        for device in ["/dev/disk0", "/dev/rdisk9", "/dev/disk9s1"]:
            with self.assertRaises(flash.UnsafeTarget):
                flash.check_disk(device)

    def test_virtual_media_rejected(self):
        with patch.object(flash, "physical_disks", return_value=[]), self.assertRaises(flash.UnsafeTarget):
            flash.check_disk("/dev/disk9")

    def test_external_apfs_system_rejected(self):
        info = dict(DeviceIdentifier="disk9", WholeDisk=True, RemovableMedia=True,
                    Writable=True, DeviceBlockSize=512)
        root = {"APFSContainerReference": "disk10", "APFSPhysicalStores": [{"APFSPhysicalStore": "disk9s2"}]}
        with patch.object(flash, "physical_disks", return_value=["/dev/disk9"]), \
                patch.object(flash.disk_tools, "disk_info", side_effect=[info, root]), \
                self.assertRaises(flash.UnsafeTarget):
            flash.check_disk("/dev/disk9")


class Workflow(unittest.TestCase):
    def exercise(self, failure=None):
        target = dict(disk="/dev/disk9", raw="/dev/rdisk9", size=512, model="SD", identity=["id"])
        commands, reads = [], []
        content = bytes(512)
        expected = flash.image_builder.sha(content)
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            image = folder / "image.img"
            image.write_bytes(content)
            image.with_suffix(".json").write_text(json.dumps({"size": 512, "sha256": expected,
                                                              "rootfs_policy": "empty reserved partition"}))
            backup_dir = folder / "work/backups"

            def read(raw, size, destination=None):
                self.assertEqual(raw, target["raw"])
                reads.append(destination)
                if destination:
                    destination.write_bytes(b"original card")
                    if failure == "backup":
                        raise flash.UnsafeTarget("backup failed")
                    return "original-hash"
                return "bad-hash" if failure == "readback" else expected

            def command(*args, **kwargs):
                commands.append(args)
                if args[:2] == ("sudo", "dd"):
                    self.assertIn("of=/dev/rdisk9", args)
                    self.assertTrue(list(backup_dir.glob("sd-*.img.sha256")))
                    self.assertEqual(len(reads), 1)
                    if failure == "write":
                        raise subprocess.CalledProcessError(1, args)

            checks = [target] * 4
            if failure == "swap":
                checks[2] = dict(target, identity=["different-card"])
            with patch.object(flash.image_builder, "verify", side_effect=
            ValueError("bad image") if failure == "image" else None), \
                    patch.object(flash, "check_disk", side_effect=checks), \
                    patch.object(flash, "read_disk", side_effect=read), \
                    patch.object(flash.disk_tools, "command", side_effect=command), \
                    patch.object(flash.shutil, "disk_usage", return_value=
                    type("Usage", (), {"free": 0 if failure == "space" else 10 ** 9})()), \
                    patch("builtins.input", return_value="cancel" if failure == "cancel" else "ERASE disk9 ALL"), \
                    contextlib.redirect_stdout(io.StringIO()):
                if failure:
                    with self.assertRaises((flash.UnsafeTarget, ValueError, subprocess.CalledProcessError)):
                        flash.flash(image, backup_dir, target)
                else:
                    flash.flash(image, backup_dir, target)
            writes = [cmd for cmd in commands if cmd[:2] == ("sudo", "dd")]
            if failure in ["image", "cancel", "space", "backup", "swap"]:
                self.assertEqual(writes, [])
            else:
                self.assertEqual(len(writes), 1)
            self.assertEqual(("diskutil", "eject", "/dev/disk9") in commands, failure is None)
            if reads:
                self.assertTrue(list(backup_dir.glob("sd-*.img")))

    def test_complete_backup_write_readback_eject(self):
        self.exercise()

    def test_failures_stop_and_keep_backup(self):
        for failure in ["image", "cancel", "space", "backup", "swap", "write", "readback"]:
            with self.subTest(failure=failure):
                self.exercise(failure)


class StreamingRead(unittest.TestCase):
    def test_successful_backup_is_exact_and_hashed(self):
        from unittest.mock import MagicMock
        data = b"card contents" * 100
        process = MagicMock()
        process.__enter__.return_value = process
        process.stdout = io.BytesIO(data)
        process.wait.return_value = 0
        with tempfile.TemporaryDirectory() as tmp, patch.object(flash.subprocess, "Popen", return_value=process):
            destination = Path(tmp) / "backup.img"
            self.assertEqual(flash.read_disk("/dev/rdisk9", len(data), destination), flash.image_builder.sha(data))
            self.assertEqual(destination.read_bytes(), data)

    def test_short_and_failed_reads_are_rejected(self):
        for data, returncode in [(b"short", 0), (bytes(512), 1), (bytes(513), 0)]:
            from unittest.mock import MagicMock
            process = MagicMock()
            process.__enter__.return_value = process
            process.stdout = io.BytesIO(data)
            process.wait.return_value = returncode
            with patch.object(flash.subprocess, "Popen", return_value=process), \
                    self.assertRaises(flash.UnsafeTarget):
                flash.read_disk("/dev/rdisk9", 512)


class EntryPoint(unittest.TestCase):
    def test_build_uses_selected_card_size_and_private_image(self):
        with tempfile.TemporaryDirectory() as tmp:
            target = dict(size=4027580416)
            args = ["flash.py", "--kernel", "kernel.bin", "--uboot", "uboot.img",
                    "--build-dir", tmp, "--backup-dir", tmp]
            with patch.object(sys, "argv", args), patch.object(sys, "platform", "darwin"), \
                    patch.object(flash, "select_disk", return_value=target), \
                    patch.object(flash.image_builder, "build") as build, \
                    patch.object(flash, "flash") as write:
                self.assertEqual(flash.main(), 0)
                self.assertEqual(build.call_args.kwargs["size_bytes"], target["size"])
                staged = build.call_args.args[2]
                self.assertEqual(staged.parent.parent, Path(tmp))
                self.assertTrue(staged.parent.name.startswith("flash-"))
                self.assertEqual(write.call_args.args[0], staged)


if __name__ == "__main__":
    unittest.main()
