"""Run with PYTHONPATH=emulator python -m pytest emulator/tests/test_fat32_image.py"""
import os
import struct
from pathlib import Path

import pytest

from lexxos_emu.fat32_export import export_fat32_snapshot
from lexxos_emu.fat32_image import CAPACITIES, create_fat32_image_from_directory


@pytest.mark.parametrize('capacity', ['256M', '16G', '32G'])
def test_sparse_roundtrip(tmp_path: Path, capacity: str) -> None:
    src = tmp_path / 'original'
    src.mkdir()
    nested = src / 'music-library'
    nested.mkdir()
    (nested / 'balanced-ganon.wav').write_bytes(b'WAV!' * 262144)
    (nested / 'empty.txt').touch()
    (src / 'LONG FILENAME (test).txt').write_text('hello', encoding='utf8')
    # Also force a directory chain longer than a single cluster on small volumes.
    for i in range(150):
        (src / f'item-{i:03d}-sample-long-name.txt').write_text(str(i))
    image = create_fat32_image_from_directory(src, tmp_path / 'sd.img', capacity=capacity)
    assert image.stat().st_size == CAPACITIES[capacity]
    assert os.stat(image).st_blocks * 512 < 32 * 1024 * 1024
    with image.open('rb') as handle:
        boot = handle.read(512)
    assert boot[82:90] == b'FAT32   '
    assert boot[510:512] == b'\x55\xAA'
    assert struct.unpack_from('<I', boot, 44)[0] == 2
    output = export_fat32_snapshot(image, tmp_path / 'exported')
    for path in src.rglob('*'):
        exported_path = output / path.relative_to(src)
        assert exported_path.exists()
        if path.is_file():
            assert path.read_bytes() == exported_path.read_bytes()


def test_export_rejects_existing_directory(tmp_path: Path) -> None:
    source = tmp_path / 'src'
    source.mkdir()
    image = create_fat32_image_from_directory(source, tmp_path / 'sd.img', capacity='256M')
    with pytest.raises(FileExistsError):
        export_fat32_snapshot(image, source)


def test_import_rejects_image_inside_source(tmp_path: Path) -> None:
    source = tmp_path / 'src'
    source.mkdir()
    with pytest.raises(ValueError):
        create_fat32_image_from_directory(source, source / 'sd.img', capacity='256M')


def test_export_includes_block_level_writes(tmp_path: Path) -> None:
    from lexxos_emu.fat32_export import Fat32Reader

    source = tmp_path / 'src'
    source.mkdir()
    (source / 'HELLO.TXT').write_bytes(b'original')
    image = create_fat32_image_from_directory(source, tmp_path / 'sd.img', capacity='256M')
    reader = Fat32Reader(image)
    try:
        item = next((record for record in reader.entries(reader.root_cluster)
                     if record[0] == 'HELLO.TXT'))
        cluster_offset = (reader.data_sector + (item[2] - 2) * reader.spc) * 512
    finally:
        reader.close()
    # Model a write_blocks() update performed by LexxOS through its block device.
    with image.open('r+b') as device:
        device.seek(cluster_offset)
        device.write(b'updated!')
    output = export_fat32_snapshot(image, tmp_path / 'output')
    assert (output / 'HELLO.TXT').read_bytes() == b'updated!'
