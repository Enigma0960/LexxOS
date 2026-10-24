"""Release archive integrity and isolation contracts."""
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('release', ROOT / 'tools/release.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class ReleaseTests(unittest.TestCase):
    def test_archive_rejects_private_paths_in_binary_and_metadata(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            package = root / 'package'
            package.mkdir()
            (package / 'firmware.elf').write_bytes(b'\x00' + str(Path.home()).encode() + b'/source.c\x00')
            with self.assertRaisesRegex(RuntimeError, 'firmware.elf'):
                release.archive(package, root / 'bad.zip', {}, 1700000000)
            self.assertFalse((root / 'bad.zip').exists())
            (package / 'firmware.elf').write_bytes(b'\x00/src/lexxos/source.c\x00')
            (package / 'manifest.json').unlink()
            (package / 'SHA256SUMS').unlink()
            with self.assertRaisesRegex(RuntimeError, 'manifest.json'):
                release.archive(package, root / 'bad.zip', {'path': str(ROOT)}, 1700000000)

    def test_release_environment_maps_source_and_sdk_paths(self):
        env = release.release_environment(Path('/tmp/source snapshot'), 1700000000)
        self.assertEqual(env['SOURCE_DATE_EPOCH'], '1700000000')
        import shlex
        for key in ('CFLAGS', 'CXXFLAGS', 'ASMFLAGS'):
            self.assertIn('-ffile-prefix-map=/tmp/source snapshot=/src/lexxos', shlex.split(env[key]))

    def test_license_fallback_requires_explicit_matching_spdx(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            component = root / 'component'
            (component / 'src').mkdir(parents=True)
            (component / 'idf_component.yml').write_text('license: Apache-2.0\n')
            (component / 'src/file.c').write_text('/* SPDX-License-Identifier: Apache-2.0 */\n')
            license_file = root / 'LICENSE'
            license_file.write_text('Apache License text')
            # SPDX is normally on its own comment line; reject unfamiliar identifiers.
            with self.assertRaises(RuntimeError):
                release.collect_licenses(component, root / 'rejected', license_file)
            (component / 'src/file.c').write_text('/*\n * SPDX-License-Identifier: Apache-2.0\n */\n')
            release.collect_licenses(component, root / 'accepted', license_file)
            self.assertTrue((root / 'accepted/src/file.c').exists())
            self.assertEqual((root / 'accepted/LICENSE').read_text(), 'Apache License text')
            (component / 'idf_component.yml').write_text('license: unknown\n')
            with self.assertRaises(RuntimeError):
                release.collect_licenses(component, root / 'unknown', license_file)

    def test_cache_parser_does_not_absorb_blank_lines(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'CMakeCache.txt'
            path.write_text('// SDK\n\nPICO_SDK_PATH:PATH=/sdk\n\nPICO_BOARD:STRING=pico\n')
            self.assertEqual(release.read_cache(path), {'PICO_SDK_PATH': '/sdk', 'PICO_BOARD': 'pico'})

    def test_archive_hashes_and_stable_zip(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            package = root / 'package'
            package.mkdir()
            (package / 'firmware.bin').write_bytes(bytes(range(256)))
            release.archive(package, root / 'a.zip', {'commit': 'abc'}, 1700000000)
            with zipfile.ZipFile(root / 'a.zip') as z:
                manifest = json.loads(z.read('package/manifest.json'))
                self.assertEqual(manifest['files']['firmware.bin'], release.sha(package / 'firmware.bin'))
                sums = z.read('package/SHA256SUMS').decode()
                self.assertIn(release.sha(package / 'manifest.json') + '  manifest.json', sums)
                self.assertIsNone(z.testzip())
            # Metadata files are regenerated, never included recursively in their own inventory.
            (package / 'manifest.json').unlink()
            (package / 'SHA256SUMS').unlink()
            release.archive(package, root / 'b.zip', {'commit': 'abc'}, 1700000000)
            self.assertEqual((root / 'a.zip').read_bytes(), (root / 'b.zip').read_bytes())

    def test_snapshot_uses_working_files_and_excludes_ignored(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / 'repo'
            root.mkdir()
            subprocess.run(['git', 'init', '-q', str(root)], check=True)
            (root / '.gitignore').write_text('build/\nCMakeUserPresets.json\n')
            (root / 'dist').mkdir()
            (root / 'dist/old.zip').write_bytes(b'old')
            (root / 'code.txt').write_text('staged')
            subprocess.run(['git', 'add', '.'], cwd=root, check=True)
            (root / 'code.txt').write_text('working')
            (root / 'new.txt').write_text('new implementation')
            (root / 'build').mkdir()
            (root / 'build/stale.bin').write_bytes(b'stale')
            (root / 'CMakeUserPresets.json').write_text('{}')
            dest = Path(tmp) / 'snapshot'
            dest.mkdir()
            release.snapshot(root, dest)
            self.assertEqual((dest / 'code.txt').read_text(), 'working')
            self.assertTrue((dest / 'new.txt').exists())
            self.assertFalse((dest / 'build').exists())
            self.assertFalse((dest / 'dist').exists())
            self.assertFalse((dest / 'CMakeUserPresets.json').exists())

    def test_matrix_includes_four_picocalc_modules_and_no_examples(self):
        data = json.loads((ROOT / 'CMakePresets.json').read_text())
        presets = {p['name']: p for p in data['configurePresets']}
        for board, sdk in [('picocalc_pico','pico'), ('picocalc_pico_w','pico_w'),
                           ('picocalc_pico2','pico2'), ('picocalc_pico2w','pico2_w')]:
            for flavor in ('minimal', 'full'):
                variables = presets[flavor+'-'+board]['cacheVariables']
                self.assertEqual(variables['PICO_BOARD'], sdk)
                self.assertEqual(variables['LEXXOS_BOARD'], board)
        self.assertEqual(len(release.BOARDS), 6)


if __name__ == '__main__':
    unittest.main()
