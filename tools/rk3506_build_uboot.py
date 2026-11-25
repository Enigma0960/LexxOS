#!/usr/bin/env python3
"""Build PicoCalc/Lyra U-Boot in Docker, without writing any device."""
import argparse
import fcntl
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request

from rk3506_fetch_uboot import URL, REVISION
from flash_rk3506_macos import checked_uboot

ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / 'ports/uboot/picocalc_lyra'


def run(*args, cwd=None):
    subprocess.run([str(a) for a in args], cwd=cwd, check=True)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def input_digest():
    paths = [PORT / name for name in (
        'Dockerfile', 'sources.json', 'integration.patch', 'install.py',
        'picocalc_splash.c', 'build.sh', 'package.py')]
    paths += [Path(__file__), ROOT / 'tools/rk3506_fetch_uboot.py',
              ROOT / 'tools/flash_rk3506_macos.py', ROOT / 'tools/rk3506_pack_fit.py']
    return sha((URL + REVISION).encode() + b''.join(
        p.name.encode() + b'\0' + p.read_bytes() + b'\0' for p in paths))


def build(folder, jobs):
    fingerprint = input_digest()
    output = folder / 'uboot.img'
    record = folder / 'build.json'
    if output.exists() and record.exists():
        metadata = json.loads(record.read_text())
        if (metadata.get('inputs') == fingerprint and
                metadata.get('sha256') == sha(output.read_bytes())):
            print(f'U-Boot unchanged; reusing {output}', flush=True)
            return
    cache = folder / 'downloads'
    cache.mkdir(exist_ok=True)
    archive = cache / f'{REVISION}.tar'
    if not archive.exists():
        with tempfile.TemporaryDirectory(dir=folder, prefix='fetch-') as tmp:
            run('git', 'init', '-q', tmp)
            run('git', 'fetch', '--depth=1', URL, REVISION, cwd=tmp)
            actual = subprocess.check_output(['git', 'rev-parse', 'FETCH_HEAD'], cwd=tmp, text=True).strip()
            if actual != REVISION:
                raise RuntimeError('Unexpected U-Boot revision')
            run('git', 'archive', '--format=tar', f'--output={archive}.tmp', 'FETCH_HEAD', cwd=tmp)
            Path(str(archive) + '.tmp').replace(archive)
    image_id = folder / 'docker-image.id'
    run('docker', 'build', '--iidfile', image_id, '-t', 'lexxos-lyra-uboot', PORT)
    docker_image = image_id.read_text().strip()
    # Build in an owned temporary checkout: never reset a user's source tree.
    with tempfile.TemporaryDirectory(dir=folder, prefix='compile-') as tmp:
        source = Path(tmp)
        with tarfile.open(archive) as tar:
            tar.extractall(source, filter='data')
        run('git', 'init', '-q', source)
        (source / '.scmversion').write_text('-lexxos')
        run(sys.executable, PORT / 'install.py', source)
        for item in json.loads((PORT / 'sources.json').read_text()):
            cached = cache / item['sha256']
            if not cached.exists() or sha(cached.read_bytes()) != item['sha256']:
                with urllib.request.urlopen(item['url'], timeout=90) as response:
                    data = response.read()
                if sha(data) != item['sha256']:
                    raise RuntimeError(f"Checksum mismatch: {item['url']}")
                cached.write_bytes(data)
            destination = source / item['destination']
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(cached, destination)
        run('docker', 'run', '--rm', '-v', f'{source}:/src', '-v', f'{PORT}:/port:ro',
            '-e', f'BUILD_JOBS={jobs}', docker_image, 'sh', '/port/build.sh')
        checked_uboot(source / 'uboot.img')
        if b'picocalc_splash' not in (source / 'u-boot-nodtb.bin').read_bytes():
            raise RuntimeError('Splash driver missing from built U-Boot')
        # Publish only a complete, validated build. Failed builds keep the old image.
        for name in ('uboot.img', 'u-boot.its', 'u-boot.dtb', 'u-boot', '.config'):
            staged = folder / (name + '.new')
            shutil.copyfile(source / name, staged)
            staged.replace(folder / name)
        metadata = dict(inputs=fingerprint, sha256=sha(output.read_bytes()),
                        uboot_revision=REVISION,
                        dependencies=json.loads((PORT / 'sources.json').read_text()))
        staged = folder / 'build.json.new'
        staged.write_text(json.dumps(metadata, indent=2) + '\n')
        staged.replace(record)
    print(f'Built {output} ({output.stat().st_size} bytes)', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    folder = args.build_dir.resolve()
    folder.mkdir(parents=True, exist_ok=True)
    with (folder / '.build.lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        build(folder, args.jobs)


if __name__ == '__main__':
    main()
