import runpy
import subprocess
import shutil
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DOCS = runpy.run_path(str(ROOT / 'tools/generate-command-docs.py'))


class CommandDocumentationTests(unittest.TestCase):
    def test_beep_backend_selection(self):
        for platform, board, expected in [
            ('pico', 'picocalc_pico_w', 'picocalc/beep_program.cpp'),
            ('pico', 'picocalc_pico2w', 'picocalc/beep_program.cpp'),
            ('sim', 'dev-sim', 'utility/beep_program.h'),
            ('espidf', 'jc_esp32p4_m3_dev', 'utility/beep_program.h'),
        ]:
            with self.subTest(board=board):
                selected = DOCS['source_docs'](ROOT, platform, board)
                self.assertTrue(selected['beep'].source.endswith(expected))
        self.assertNotIn('beep', DOCS['source_docs'](ROOT, 'host', 'host_native'))

    def test_ambiguous_metadata_is_error(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'src').mkdir()
            for name in ('first.h', 'second.h'):
                (root / 'src' / name).write_text('/**\n * @lexxos-command-doc duplicate\n */')
            with self.assertRaisesRegex(ValueError, 'Ambiguous documentation for duplicate'):
                DOCS['source_docs'](root, 'host', 'host_native')

    def test_disabled_commands_excluded(self):
        self.assertEqual(DOCS['parse_command_list']('help - show help\ngpio-analyzer (disabled) - graph'), {'help': 'show help'})

    def test_stale_pages_not_in_index_and_stale_metadata_not_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            (output / 'obsolete.md').write_text('# obsolete\n')
            (output / 'beep.md').write_text('# beep\n\n## Параметры\n\nwrong backend\n\n## Заметки\n\nmanual note\n')
            command = DOCS['CommandDoc']('beep', 'tone', 'Usage: beep', 'sim / full')
            DOCS['write_documents'](output, {'beep': command})
            self.assertNotIn('obsolete', (output / 'README.md').read_text())
            self.assertTrue((output / 'obsolete.md').exists())
            self.assertNotIn('wrong backend', (output / 'beep.md').read_text())
            self.assertIn('manual note', (output / 'beep.md').read_text())

    def test_missing_build_identity_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'CMakeCache.txt').write_text('LEXXOS_PLATFORM:STRING=host\n')
            with self.assertRaisesRegex(ValueError, 'LEXXOS_BOARD'):
                DOCS['read_build_context'](root)

    @unittest.skipUnless(shutil.which('cmake'), 'CMake required')
    def test_analyzer_full_only_even_with_cached_override(self):
        with tempfile.TemporaryDirectory() as directory:
            script = Path(directory) / 'features.cmake'
            for flavor, expected in [('minimal', 'OFF'), ('standard', 'OFF'), ('full', 'ON')]:
                script.write_text(f"""set(LEXXOS_BUILD_FLAVOR {flavor} CACHE STRING "")
set(LEXXOS_FEATURE_GPIO_ANALYZER ON CACHE BOOL "")
include("{ROOT / 'cmake/Features.cmake'}")
if (NOT LEXXOS_FEATURE_GPIO_ANALYZER STREQUAL "{expected}")
    message(FATAL_ERROR "Unexpected analyzer feature")
endif ()
""")
                result = subprocess.run(['cmake', '-P', str(script)], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
