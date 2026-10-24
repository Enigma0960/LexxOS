#!/usr/bin/env python3
"""Build isolated release archives. Never flashes hardware or publishes a release."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time
import zipfile

ROOT = Path(__file__).resolve().parents[1]
BOARDS = ('m5stack_cardputer_adv', 'picocalc_pico', 'picocalc_pico_w',
          'picocalc_pico2', 'picocalc_pico2w', 'jc_esp32p4_m3_dev')


def output(cmd, cwd=ROOT):
    return subprocess.check_output(cmd, cwd=cwd, text=True).strip()


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + '\n')


def run(cmd, cwd, log, env):
    print('+', ' '.join(map(str, cmd)), flush=True)
    with log.open('w') as stream:
        result = subprocess.run(list(map(str, cmd)), cwd=cwd, env=env,
                                stdout=stream, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f'Command failed ({result.returncode}); log: {log}\n'
                           + log.read_text(errors='replace')[-5000:])


def copy_file(source, target):
    if not source.is_file():
        raise RuntimeError(f'Missing artifact: {source}')
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)


def snapshot(root, destination):
    """Copy working files, not the index; never copy ignored caches or user presets."""
    names = output(['git', 'ls-files', '--cached', '--others', '--exclude-standard', '-z'], root).split('\0')
    for name in names:
        source = root / name
        if not name or name == 'CMakeUserPresets.json' or Path(name).parts[0] in {'build', 'dist'}:
            continue
        if source.is_file():
            copy_file(source, destination / name)
    # Submodule contents are not listed by the parent repository.
    for line in output(['git', 'submodule', 'status', '--recursive'], root).splitlines():
        if line[0] in '-+U':
            raise RuntimeError('Initialize submodules at their recorded commits first: ' + line)
        name = line.split()[1]
        if output(['git', 'status', '--porcelain', '--untracked-files=normal'], root / name):
            raise RuntimeError('Dirty submodule: ' + name)
        for file in output(['git', 'ls-files', '-z'], root / name).split('\0'):
            if file and (root / name / file).is_file():
                copy_file(root / name / file, destination / name / file)
    digest = hashlib.sha256()
    for p in sorted(destination.rglob('*')):
        if p.is_file():
            digest.update(p.relative_to(destination).as_posix().encode() + b'\0' + p.read_bytes())
    return digest.hexdigest()


def collect_licenses(source, destination, apache_license=None):
    """Keep upstream notices verbatim, including nested SDK dependency notices."""
    count = 0
    for directory, dirs, files in os.walk(source):
        dirs[:] = [d for d in dirs if d not in {'.git', 'build', '.venv', '__pycache__', 'node_modules'}]
        for name in files:
            p = Path(directory) / name
            lower = name.lower()
            if lower.startswith(('license', 'licence', 'copying', 'copyright', 'notice')) or 'licenses' in p.relative_to(source).parts:
                copy_file(p, destination / p.relative_to(source))
                count += 1
    if not count:
        component = source / 'idf_component.yml'
        declaration = component.read_text() if component.is_file() else ''
        # Some Espressif components ship SPDX notices but no standalone LICENSE.
        # Accept only the explicit Apache declaration, preserving the source notices.
        declared_apache = re.search(r"(?m)^license: *['\"]?Apache-2\.0['\"]? *$", declaration)
        sources = sorted((source / 'src').rglob('*')) if (source / 'src').is_dir() else []
        texts = [p for p in sources if p.is_file() and p.suffix in {'.c', '.cpp', '.h', '.hpp'}]
        identifiers = [ident for p in texts for ident in re.findall(
            r'SPDX-License-Identifier: ([^\r\n]+)', p.read_text(errors='replace'))]
        if not (declared_apache and apache_license and identifiers
                and all(ident.strip() == 'Apache-2.0' for ident in identifiers)):
            raise RuntimeError(f'No license notices found in dependency: {source}')
        copy_file(apache_license, destination / 'LICENSE')
        copy_file(component, destination / 'idf_component.yml')
        for path in texts:
            copy_file(path, destination / path.relative_to(source))



def common(source, package):
    for name in ('LICENSE', 'NOTICE', 'THIRD_PARTY_NOTICES.md'):
        copy_file(source / name, package / name)
    shutil.copytree(source / 'licenses', package / 'licenses/project')


def archive(package, archive_path, metadata, epoch):
    files = {p.relative_to(package).as_posix(): sha(p)
             for p in sorted(package.rglob('*')) if p.is_file()}
    write_json(package / 'manifest.json', dict(metadata, packager_sha256=sha(Path(__file__)), files=files))
    files['manifest.json'] = sha(package / 'manifest.json')
    (package / 'SHA256SUMS').write_text(''.join(f'{value}  {name}\n' for name, value in sorted(files.items())))
    stamp = time.gmtime(max(epoch, 315532800))[:6]
    with zipfile.ZipFile(archive_path, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for p in sorted(package.rglob('*')):
            if p.is_file():
                info = zipfile.ZipInfo(package.name + '/' + p.relative_to(package).as_posix(), stamp)
                info.external_attr = 0o100644 << 16
                z.writestr(info, p.read_bytes(), compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)
    with zipfile.ZipFile(archive_path) as z:
        if z.testzip():
            raise RuntimeError('Archive verification failed: ' + str(archive_path))


def read_cache(path):
    return dict(re.findall(r'^([A-Za-z_][A-Za-z0-9_]*):[^=\n]*=(.*)$', path.read_text(), re.M))


def package_firmware(source, build, package, board, metadata):
    common(source, package)
    collect_licenses(source / 'third_party/FreeRTOS', package / 'licenses/FreeRTOS')
    collect_licenses(source / 'third_party/fatfs', package / 'licenses/FatFs')
    cache = read_cache(build / 'CMakeCache.txt')
    metadata['compiler'] = {}
    for key in ('CMAKE_C_COMPILER', 'CMAKE_CXX_COMPILER'):
        if key in cache:
            metadata['compiler'][key] = output([cache[key], '--version']).splitlines()[0]
    if board.startswith('picocalc'):
        copy_file(build / 'src/lexxos.uf2', package / 'firmware.uf2')
        copy_file(build / 'src/lexxos.elf', package / 'firmware.elf')
        sdk = Path(cache['PICO_SDK_PATH'])
        collect_licenses(sdk, package / 'licenses/pico-sdk')
        metadata['pico_board'] = cache['PICO_BOARD']
        version_file = sdk / 'pico_sdk_version.cmake'
        metadata['pico_sdk_version'] = {k.lower(): v for k, v in re.findall(r'set\(PICO_SDK_VERSION_(MAJOR|MINOR|REVISION) (\d+)\)', version_file.read_text())}
        metadata['keyboard_firmware'] = cache.get('LEXXOS_PICOCALC_KEYBOARD_FIRMWARE')
        (package / 'INSTALL.txt').write_text(
            f'LexxOS for {board}\nUse only with the indicated Pico module.\n'
            f'PicoCalc southbridge firmware: {metadata["keyboard_firmware"]}.\n'
            'Hold BOOTSEL while connecting the module over USB, then copy firmware.uf2 '
            'to the boot drive. Do not copy it to the SD card.\n'
            'Wi-Fi is available only on W modules. Hardware regression is separate from a successful build.\n')
    else:
        args = json.loads((build / 'flasher_args.json').read_text())
        description = json.loads((build / 'project_description.json').read_text())
        idf = Path(description['idf_path'])
        collect_licenses(idf, package / 'licenses/esp-idf')
        for name, component in zip(description['build_components'], description['build_component_paths']):
            path = Path(component)
            if 'managed_components' in path.parts:
                collect_licenses(path, package / 'licenses/managed' / name, idf / 'LICENSE')
        version_text = (idf / 'tools/cmake/version.cmake').read_text()
        version_parts = dict(re.findall(r'set\(IDF_VERSION_(MAJOR|MINOR|PATCH) (\d+)\)', version_text))
        metadata['idf_version'] = '.'.join(version_parts[k] for k in ('MAJOR', 'MINOR', 'PATCH'))
        compiler = Path(cache.get('CMAKE_C_COMPILER', ''))
        toolchain_notices = compiler.parent.parent / 'share/licenses'
        if toolchain_notices.is_dir():
            shutil.copytree(toolchain_notices, package / 'licenses/toolchain-runtime')
        portable = []
        for offset, name in args['flash_files'].items():
            path = Path(name)
            if path.is_absolute() or '..' in path.parts:
                raise RuntimeError('Unexpected flash artifact path: ' + name)
            copy_file(build / path, package / path)
            portable.extend([offset, path.as_posix()])
        copy_file(build / 'LexxOS.elf', package / 'LexxOS.elf')
        copy_file(build / 'sdkconfig', package / 'sdkconfig')
        write_json(package / 'flash.json', args)
        chip = args['extra_esptool_args']['chip']
        command = 'python -m esptool --chip ' + chip + ' --port PORT write-flash ' + ' '.join(args['write_flash_args'] + portable)
        (package / 'INSTALL.txt').write_text(
            f'LexxOS for {board}\nInstall esptool 5.x in your Python environment.\n'
            'Extract this archive, open a terminal in its directory, replace PORT with the serial port.\n'
            + command + '\nWrites bootloader, partition table and application. No erase-flash is required.\n'
            'Back up existing board flash data before changing partition layouts. SD is not formatted.\n'
            'Does not update the JC ESP32-C6 coprocessor. Use compatible Hosted firmware separately.\n'
            'Hardware regression is separate from a successful build.\n')
    for name in ('board_list.h', 'feature_list.h', 'commands/commands.json'):
        copy_file(build / 'generated/include' / name, package / 'configuration' / name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--boards', nargs='+', choices=BOARDS, default=list(BOARDS))
    parser.add_argument('--flavors', nargs='+', choices=('minimal', 'full'), default=['minimal', 'full'])
    parser.add_argument('--apps-only', action='store_true')
    parser.add_argument('--allow-dirty', action='store_true', help='Create explicitly marked development archives from working files')
    parser.add_argument('--output', type=Path, default=ROOT / 'dist')
    parser.add_argument('--jobs', type=int, default=min(8, os.cpu_count() or 2))
    parser.add_argument('--plan', action='store_true')
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    profiles = [] if args.apps_only else [f'{flavor}-{board}' for board in dict.fromkeys(args.boards) for flavor in dict.fromkeys(args.flavors)]
    if args.plan:
        print('\n'.join(profiles + ['apps']))
        return
    dirty = output(['git', 'status', '--porcelain', '--untracked-files=normal'])
    if dirty and not args.allow_dirty:
        raise RuntimeError('Release requires a clean working tree and index. Commit changes first, or use --allow-dirty for development archives.')
    version = re.search(r'set\(LEXXOS_VERSION "([^"]+)"\)', (ROOT / 'cmake/Version.cmake').read_text())[1]
    commit = output(['git', 'rev-parse', 'HEAD'])
    epoch = int(output(['git', 'show', '-s', '--format=%ct', 'HEAD']))
    base = ROOT / 'build/release-runs';base.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='run-', dir=base))
    source = work / 'source';source.mkdir()
    digest = snapshot(ROOT, source)
    label = version + ('-dev-' + digest[:12] if dirty else '')
    destination = args.output.resolve() / ('LexxOS-' + label)
    if destination.exists():
        raise RuntimeError('Output already exists; choose another --output: ' + str(destination))
    env = os.environ.copy();env['SOURCE_DATE_EPOCH'] = str(epoch)
    staged = work / 'archives';staged.mkdir()
    meta = {'version':version, 'commit':commit, 'dirty':bool(dirty), 'source_sha256':digest,
            'source_date_epoch':epoch, 'cmake':output(['cmake','--version']).splitlines()[0]}
    print('Build logs and sources:', work, flush=True)
    for profile in profiles:
        board = profile.split('-', 1)[1]
        build = source / 'build' / profile
        run(['cmake', '--preset', profile, '-DLEXXOS_APPS=OFF'], source, work / (profile+'-configure.log'), env)
        run(['cmake', '--build', build, '--parallel', args.jobs], source, work / (profile+'-build.log'), env)
        package = work / ('LexxOS-'+label+'-'+profile);package.mkdir()
        detail = dict(meta, profile=profile, board=board)
        package_firmware(source, build, package, board, detail)
        archive(package, staged / (package.name+'.zip'), detail, epoch)
    run(['cmake','--preset','apps'],source,work/'apps-configure.log',env)
    run(['cmake','--build','--preset','apps','--parallel',args.jobs],source,work/'apps-build.log',env)
    package = work / ('LexxOS-'+label+'-apps');package.mkdir();common(source, package)
    apps = sorted((source/'build/apps').glob('*.lexx'))
    if not apps:
        raise RuntimeError('No apps built')
    for path in apps:
        if not (source/'apps'/path.stem/'CMakeLists.txt').is_file():
            raise RuntimeError('Unexpected executable outside apps/: '+path.name)
        copy_file(path, package/'lexx'/path.name)
    (package/'INSTALL.txt').write_text('Copy lexx/ to the root of the SD card. Run /mnt/sd/lexx/NAME.lexx from LexxOS.\nOnly applications built from apps/ are included; examples/ are excluded.\n')
    archive(package, staged/(package.name+'.zip'),dict(meta,profile='apps'),epoch)
    (staged/'SHA256SUMS').write_text(''.join(f'{sha(p)}  {p.name}\n' for p in sorted(staged.glob('*.zip'))))
    write_json(staged/'release.json',dict(meta,profiles=profiles+['apps'],archives=[p.name for p in sorted(staged.glob('*.zip'))]))
    destination.parent.mkdir(parents=True,exist_ok=True)
    # Destination appears only after the complete selected matrix has succeeded.
    incoming = Path(tempfile.mkdtemp(prefix='.lexxos-',dir=destination.parent))
    try:
        shutil.copytree(staged,incoming,dirs_exist_ok=True)
        incoming.rename(destination)
    finally:
        if incoming.exists():shutil.rmtree(incoming)
    print('Release archives:',destination)


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError, OSError) as error:
        sys.exit(str(error))
