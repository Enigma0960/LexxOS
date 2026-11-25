#!/usr/bin/env python3
"""Host-only contract tests: no board or GDB required."""
import importlib.util
from pathlib import Path
import unittest
from unittest import mock
import tempfile
import contextlib
import io

module = Path(__file__).with_name('rk3506_debug_reload.py')
spec = importlib.util.spec_from_file_location('reload_rk3506', module)
reload = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reload)


class LoaderContract(unittest.TestCase):
    def test_generated_script_requires_swd_and_preflight_before_load(self):
        script = reload.make_gdb_script(Path('/tmp/kernel.elf'), Path('/tmp/monitor.bin'),
                                        0x027F00D0, '127.0.0.1', 3333)
        self.assertLess(script.index('0xff950014'), script.index('restore '))
        self.assertLess(script.index('restore '), script.index('continue'))
        self.assertLess(script.index('if $reload_status !='), script.index('\nload\n'))
        self.assertLess(script.index('\nload\n'), script.index('monitor resume'))
        self.assertIn('quit 22', script)
        self.assertNotIn('reset init', script)
        self.assertNotIn('flash write_image', script)
        self.assertIn('disconnect', script)

    def test_restore_uses_unquoted_basename_for_gdb_parser(self):
        script = reload.make_gdb_script(Path('/tmp/kernel.elf'),
                                        Path('/tmp/path with spaces/monitor.bin'),
                                        0x027F00D0, '127.0.0.1', 3333)
        self.assertIn('restore monitor.bin binary 0x027f0000', script)
        self.assertNotIn('restore "', script)
        self.assertNotIn('/tmp/path with spaces', script)

    def test_monitor_region_and_expected_magic(self):
        self.assertEqual(reload.MONITOR_ADDR, 0x027F0000)
        self.assertEqual(reload.STATUS_ADDR, 0x027F0200)
        self.assertEqual(reload.READY, 0x52414D4F)

    def test_attach_handoff_stays_at_entry_point(self):
        script = reload.make_gdb_script(Path('/tmp/kernel.elf'), Path('/tmp/monitor.bin'),
                                        0x027F00D0, '127.0.0.1', 3333, stay_halted=True)
        self.assertNotIn('monitor resume', script)
        self.assertIn('set $pc = 0x02000000\n', script)
        self.assertLess(script.index('arm mrc'), script.index('restore '))
        self.assertLess(script.index('arm mcr 15 0 7 5 0 0'), script.index('restore '))
        self.assertLess(script.index('set $pc = 0x02000000'), script.index('disconnect'))

    def test_overlapping_image_never_starts_gdb(self):
        with tempfile.NamedTemporaryFile(suffix='.elf') as elf:
            with mock.patch('sys.argv', ['reload', '--elf', elf.name]), \
                    mock.patch.object(reload, 'symbol', side_effect=[0x02000000, reload.MONITOR_ADDR + 1]), \
                    mock.patch.object(reload, 'run') as command, \
                    contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(reload.main(), 1)
                command.assert_not_called()

    def test_dry_run_never_builds_or_connects(self):
        with mock.patch('sys.argv', ['reload', '--dry-run', '--stay-halted',
                                     '--configure-preset', 'picocalc-lyra-debug']), \
                mock.patch.object(reload, 'run') as command, \
                contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(reload.main(), 0)
            command.assert_not_called()


if __name__ == '__main__':
    unittest.main()
