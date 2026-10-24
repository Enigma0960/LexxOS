"""Contract tests for selection, source exclusion and platform restrictions."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('catalog', ROOT / 'tools/generate_command_catalog.py')
catalog = importlib.util.module_from_spec(spec)
spec.loader.exec_module(catalog)
ENTRIES = json.loads((ROOT / 'cmake/commands.json').read_text())


class CommandCatalogTests(unittest.TestCase):
    def select(self, board='dev-sim', platform='sim', features=(), groups=None, backend='sim'):
        if groups is None:
            groups = catalog.GROUPS
        flags = set(features) | {'GROUP_' + group for group in groups}
        flags = catalog.capabilities(board, platform, 'full', backend, flags)
        return catalog.select(ENTRIES, flags), flags

    def test_no_tests_even_when_device_test_flags_are_cached_on(self):
        selected, _ = self.select(features={'DISPLAY', 'DISPLAY_TEST_COMMANDS', 'SD_CARD',
                                            'SD_CARD_TEST_COMMANDS', 'GAMEBOY'},
                                  groups=catalog.GROUPS - {'TESTS'})
        names = {e['name'] for e in selected}
        self.assertTrue({'display-info', 'gbemu', 'help'} <= names)
        self.assertFalse({'bench', 'fs-smoke', 'sd-test', 'gb-test-suite', 'display-test'} & names)

    def test_no_display_does_not_remove_uart_utilities(self):
        selected, _ = self.select(board='jc_esp32p4_m3_dev', platform='espidf')
        names = {e['name'] for e in selected}
        self.assertTrue({'edit', 'echo', 'clear', 'cat'} <= names)
        self.assertFalse(any(n.startswith(('display-', 'touch-')) for n in names))
        self.assertNotIn('console-info', names)

    def test_backend_and_board_restrictions(self):
        selected, _ = self.select(board='jc_esp32p4_m3_dev', platform='espidf',
                                  features={'SD_CARD', 'SD_CARD_TEST_COMMANDS'}, backend='sdio')
        names = {e['name'] for e in selected}
        self.assertIn('sd-test', names)
        self.assertNotIn('wifi-connect', names)  # P4 has no native Wi-Fi without Hosted.
        self.assertFalse({'sd-spi-mode', 'sd-speed-scan', 'bootsel', 'exit', 'esp32-spi-test'} & names)
        selected, _ = self.select(board='m5stack_cardputer', platform='espidf')
        self.assertFalse({'mic', 'audio', 'imu', 'ir', 'eth'} & {e['name'] for e in selected})

    def test_non_wifi_pico_does_not_inherit_sdk_wifi_target(self):
        selected, _ = self.select(board="dev_pico", platform="pico", features={"CYW43"})
        self.assertNotIn("wifi-connect", {e["name"] for e in selected})
        self.assertNotIn("net-status", {e["name"] for e in selected})

    def test_heap_gameboy_on_desktop_but_not_pico_without_psram(self):
        for platform, board in [('host', 'host_native'), ('sim', 'dev-sim'), ('pico', 'dev_pico')]:
            selected, _ = self.select(board=board, platform=platform, features={'GAMEBOY'})
            self.assertEqual('gbemu' in {e['name'] for e in selected}, platform != 'pico')

    def test_private_sources_and_registration_are_both_removed(self):
        selected, flags = self.select(groups={'SYSTEM'})
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            catalog.generate(ENTRIES, selected, output, flags)
            sources = (output / 'command_sources.cmake').read_text().split('set(LEXXOS_SELECTED_COMMAND_SOURCES')[1]
            registration = (output / 'builtin_commands.inc').read_text()
            self.assertNotIn('edit_program.cpp', sources)
            self.assertNotIn('editor_controller.cpp', sources)
            self.assertNotIn('midi_renderer.cpp', sources)
            self.assertNotIn('BenchmarkProgram', registration)
            self.assertNotIn('EditProgram', registration)

    def test_manifest_files_and_documentation_exist(self):
        for entry in ENTRIES:
            self.assertIn(entry['group'], catalog.GROUPS)
            for path in [entry['header'], *entry['sources']]:
                self.assertTrue((ROOT / 'src' / path).is_file(), path)
            self.assertTrue((ROOT / entry['doc']).is_file(), entry['doc'])

    def test_unknown_dependency_is_an_error(self):
        entry = dict(ENTRIES[0], requires=["DISPALY"])
        with self.assertRaisesRegex(ValueError, "Unknown dependencies"):
            catalog.select([entry], set())

    def test_platform_beep_variants_do_not_collide(self):
        for board, platform in [('dev-sim', 'sim'), ('picocalc_pico2w', 'pico')]:
            selected, _ = self.select(board=board, platform=platform)
            self.assertEqual(sum(e['name'] == 'beep' for e in selected), 1)


if __name__ == '__main__':
    unittest.main()
