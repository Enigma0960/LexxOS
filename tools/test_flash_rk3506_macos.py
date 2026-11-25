#!/usr/bin/env python3
"""Exercise conditional U-Boot writes without opening a device or running sudo."""
import contextlib
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import flash_rk3506_macos as flash


class UbootUpdate(unittest.TestCase):
    def exercise(self, existing, *, corrupt=False, replacement=False):
        image = b'new firmware'
        expected = image.ljust(flash.UBOOT_SIZE, b'\0')
        target = dict(disk='/dev/disk9', boot='/dev/rdisk9s2', uuid='boot-id',
                      uboot='/dev/rdisk9s1', uboot_uuid='uboot-id')
        writes = []
        reads = []

        def read(raw, destination, size):
            self.assertEqual(raw, target['uboot'])
            self.assertEqual(size, flash.UBOOT_SIZE)
            reads.append(raw)
            destination.write_bytes(existing if len(reads) == 1 else
                                    (b'bad' if corrupt else expected))

        def command(*args, **kwargs):
            if args[:2] == ('sudo', 'dd'):
                self.assertIn('of=/dev/rdisk9s1', args)
                staged = Path(next(a[3:] for a in args if a.startswith('if=')))
                self.assertEqual(staged.read_bytes(), expected)
                self.assertEqual(len(list(backup.glob('uboot-*.img'))), 1)
                writes.append(args)

        with tempfile.TemporaryDirectory() as folder:
            backup = Path(folder)
            cards = [target, dict(target, uboot_uuid='different')] if replacement else None
            with patch.object(flash, 'check_disk', return_value=target, side_effect=cards), \
                    patch.object(flash, 'sudo_read_partition', side_effect=read), \
                    patch.object(flash, 'command', side_effect=command), \
                    contextlib.redirect_stdout(io.StringIO()):
                if corrupt or replacement:
                    with self.assertRaises(flash.UnsafeTarget):
                        flash.update_uboot(image, backup, target, 'test')
                else:
                    changed = flash.update_uboot(image, backup, target, 'test')
                    self.assertEqual(changed, existing != expected)
                saved = list(backup.glob('uboot-*.img'))
                if saved:
                    self.assertEqual(saved[0].read_bytes(), existing)
        return writes, reads

    def test_identical_partition_has_no_write_or_backup(self):
        writes, reads = self.exercise(b'new firmware'.ljust(flash.UBOOT_SIZE, b'\0'))
        self.assertEqual(writes, [])
        self.assertEqual(len(reads), 1)

    def test_changed_partition_is_backed_up_written_and_verified(self):
        writes, reads = self.exercise(b'old'.ljust(flash.UBOOT_SIZE, b'\0'))
        self.assertEqual(len(writes), 1)
        self.assertEqual(len(reads), 2)

    def test_stale_trailing_copy_requires_update(self):
        data = bytearray(b'new firmware'.ljust(flash.UBOOT_SIZE, b'\0'))
        data[-1] = 1
        self.assertEqual(len(self.exercise(bytes(data))[0]), 1)

    def test_replaced_card_is_never_written(self):
        self.assertEqual(self.exercise(b'old', replacement=True)[0], [])

    def test_failed_readback_raises_and_preserves_backup(self):
        self.assertEqual(len(self.exercise(b'old', corrupt=True)[0]), 1)

    def test_kernel_fit_cannot_be_passed_as_uboot(self):
        path = Path(__file__).resolve().parents[1] / 'third_party/rk3506-luckfox-lurra/boot.img'
        if not path.exists():
            self.skipTest('optional vendor template absent')
        with self.assertRaises(flash.UnsafeTarget):
            flash.checked_uboot(path)


class FlashWorkflow(unittest.TestCase):
    def exercise(self, with_uboot, fail_uboot=False):
        target = dict(disk='/dev/disk9', boot='/dev/rdisk9s2', uuid='boot-id',
                      uboot='/dev/rdisk9s1', uboot_uuid='uboot-id')
        image = bytes.fromhex('d00dfeed') + b'validated image'
        commands = []

        def read(raw, destination, size=flash.BOOT_SIZE):
            self.assertEqual(raw, target['boot'])
            destination.write_bytes(image.ljust(size, b'\0'))

        def command(*args, **kwargs):
            commands.append(args)
            if args[:2] == ('sudo', 'dd'):
                self.assertIn('of=/dev/rdisk9s2', args)
                staged = Path(next(a[3:] for a in args if a.startswith('if=')))
                self.assertEqual(staged.read_bytes(), image)

        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / 'source.img'
            source.write_bytes(b'replaced after validation')
            with patch.object(flash, 'check_disk', return_value=target), \
                    patch.object(flash, 'sudo_read_partition', side_effect=read), \
                    patch.object(flash, 'command', side_effect=command), \
                    patch.object(flash, 'update_uboot', side_effect=
                                 flash.UnsafeTarget('failed') if fail_uboot else None) as update, \
                    patch('builtins.input', return_value='FLASH disk9s2' +
                          (' + UBOOT' if with_uboot else '')), \
                    contextlib.redirect_stdout(io.StringIO()):
                if fail_uboot:
                    with self.assertRaises(flash.UnsafeTarget):
                        flash.flash(source, Path(folder), target, image, b'uboot')
                    self.assertFalse(any(c[:2] == ('sudo', 'dd') for c in commands))
                    self.assertNotIn(('diskutil', 'eject', '/dev/disk9'), commands)
                else:
                    flash.flash(source, Path(folder), target, image,
                                b'uboot' if with_uboot else None)
                    self.assertIn(('diskutil', 'eject', '/dev/disk9'), commands)
                self.assertEqual(update.call_count, int(with_uboot))

    def test_without_uboot_preserves_boot_only_workflow(self):
        self.exercise(False)

    def test_with_uboot_uses_conditional_update_and_validated_boot_bytes(self):
        self.exercise(True)

    def test_failed_uboot_update_stops_before_boot_write(self):
        self.exercise(True, fail_uboot=True)


if __name__ == '__main__':
    unittest.main()
