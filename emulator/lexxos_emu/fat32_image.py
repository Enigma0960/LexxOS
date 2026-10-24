"""Sparse, streaming FAT32 superfloppy image generator for the LexxOS simulator.

Only Python's standard library is required. The generated volume starts at LBA 0
(the same layout FatFs already accepts for its physical-drive path).
"""
from __future__ import annotations

import math
import os
import struct
import tempfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import BinaryIO

from ._fat16_names import (
    create_directory_entry,
    create_dot_entry,
    make_unique_short_alias,
)

SECTOR_SIZE = 512
EOC = 0x0FFFFFFF
RESERVED_SECTORS = 32
FAT_COUNT = 2
ROOT_CLUSTER = 2
# FAT32 stores a file size in 32 bits; a single file must fit in 0xFFFFFFFF.
MAX_FILE_SIZE = 0xFFFFFFFF
# LexxOS targets 512-byte sectors and ordinary FAT32 cluster sizes.
CAPACITIES = {"256M": 256 << 20, "512M": 512 << 20,
              "16G": 16 << 30, "32G": 32 << 30}


@dataclass
class Node:
    path: Path
    name: str
    is_dir: bool
    size: int = 0
    children: list["Node"] = field(default_factory=list, repr=False)
    first_cluster: int = 0
    cluster_count: int = 0


def _scan(path: Path) -> Node:
    if path.is_symlink():
        raise ValueError(f"symlink cannot be imported: {path}")
    if path.is_dir():
        children = []
        for child in sorted(path.iterdir(), key=lambda p: (p.name.casefold(), p.name)):
            # Preserve the earlier importer's policy: do not follow links.
            if child.is_symlink():
                continue
            if child.is_dir() or child.is_file():
                children.append(_scan(child))
        return Node(path, path.name, True, children=children)
    size = path.stat().st_size
    if size > MAX_FILE_SIZE:
        raise ValueError(f"FAT32 supports files up to 4 GiB - 1 byte: {path}")
    return Node(path, path.name, False, size=size)


def _fat32_entry(name: str, alias: str, *, attribute: int,
                 first_cluster: int, size: int) -> bytes:
    raw = bytearray(create_directory_entry(
        name, alias, attribute=attribute,
        first_cluster=first_cluster & 0xFFFF, size=size))
    # A FAT32 entry stores the high 16 bits at offset 20 in the *short* record.
    struct.pack_into('<H', raw, len(raw) - 32 + 20, first_cluster >> 16)
    return bytes(raw)


def _dot_entry(name: str, cluster: int) -> bytes:
    record = bytearray(create_dot_entry(name, cluster & 0xFFFF))
    struct.pack_into('<H', record, 20, cluster >> 16)
    return bytes(record)


def _directory_bytes(node: Node, *, root: bool, parent_cluster: int = 0) -> bytes:
    result = bytearray()
    if not root:
        result.extend(_dot_entry('.', node.first_cluster))
        result.extend(_dot_entry('..', parent_cluster))
    aliases: set[str] = set()
    for child in node.children:
        alias = make_unique_short_alias(child.name, aliases)
        result.extend(_fat32_entry(
            child.name, alias,
            attribute=0x10 if child.is_dir else 0x20,
            first_cluster=child.first_cluster,
            size=0 if child.is_dir else child.size))
    return bytes(result)


class Fat32Layout:
    def __init__(self, size_bytes: int, sectors_per_cluster: int):
        if size_bytes % SECTOR_SIZE or size_bytes // SECTOR_SIZE > 0xFFFFFFFF:
            raise ValueError('FAT32 requires a sector-aligned volume <= 2 TiB')
        if sectors_per_cluster not in (1, 2, 4, 8, 16, 32, 64):
            raise ValueError('unsupported sectors_per_cluster for FAT32')
        self.total_sectors = size_bytes // SECTOR_SIZE
        self.spc = sectors_per_cluster
        self.cluster_size = self.spc * SECTOR_SIZE
        self.fat_sectors = 1
        while True:
            count = (self.total_sectors - RESERVED_SECTORS - FAT_COUNT * self.fat_sectors) // self.spc
            needed = math.ceil((count + 2) * 4 / SECTOR_SIZE)
            if needed <= self.fat_sectors:
                break
            self.fat_sectors = needed
        self.data_sector = RESERVED_SECTORS + FAT_COUNT * self.fat_sectors
        self.cluster_count = (self.total_sectors - self.data_sector) // self.spc
        if not (65525 <= self.cluster_count <= 0x0FFFFFF5):
            raise ValueError('FAT32 volume has an invalid number of clusters')
        self.fat = bytearray(self.fat_sectors * SECTOR_SIZE)
        struct.pack_into('<II', self.fat, 0, 0x0FFFFFF8, EOC)
        self.next_cluster = ROOT_CLUSTER

    def allocate(self, count: int) -> int:
        if count == 0:
            return 0
        first = self.next_cluster
        if first + count > self.cluster_count + 2:
            free = self.cluster_count + 2 - first
            raise ValueError(f'FAT32 image is full (need {count} clusters, available {free})')
        for index in range(count):
            cluster = first + index
            value = cluster + 1 if index + 1 < count else EOC
            struct.pack_into('<I', self.fat, 4 * cluster, value)
        self.next_cluster += count
        return first

    def cluster_offset(self, cluster: int) -> int:
        return (self.data_sector + (cluster - 2) * self.spc) * SECTOR_SIZE

    def write_metadata(self, image: BinaryIO) -> None:
        boot = bytearray(SECTOR_SIZE)
        boot[0:3] = b'\xEB\x58\x90'
        boot[3:11] = b'MSWIN4.1'
        struct.pack_into('<H', boot, 11, SECTOR_SIZE)
        boot[13] = self.spc
        struct.pack_into('<H', boot, 14, RESERVED_SECTORS)
        boot[16] = FAT_COUNT
        boot[21] = 0xF8
        struct.pack_into('<H', boot, 24, 63)
        struct.pack_into('<H', boot, 26, 255)
        struct.pack_into('<I', boot, 32, self.total_sectors)
        struct.pack_into('<I', boot, 36, self.fat_sectors)
        struct.pack_into('<I', boot, 44, ROOT_CLUSTER)
        struct.pack_into('<H', boot, 48, 1)  # FSInfo
        struct.pack_into('<H', boot, 50, 6)  # backup boot
        boot[64] = 0x80
        boot[66] = 0x29
        struct.pack_into('<I', boot, 67, 0x4C584F53)
        boot[71:82] = b'LEXXOS     '
        boot[82:90] = b'FAT32   '
        boot[510:512] = b'\x55\xAA'
        info = bytearray(SECTOR_SIZE)
        struct.pack_into('<I', info, 0, 0x41615252)
        struct.pack_into('<I', info, 484, 0x61417272)
        struct.pack_into('<I', info, 488, self.cluster_count - (self.next_cluster - 2))
        struct.pack_into('<I', info, 492,
                         self.next_cluster if self.next_cluster <= self.cluster_count + 1 else 0xFFFFFFFF)
        struct.pack_into('<I', info, 508, 0xAA550000)
        for sector, data in ((0, boot), (1, info), (6, boot), (7, info)):
            image.seek(sector * SECTOR_SIZE)
            image.write(data)
        for index in range(FAT_COUNT):
            image.seek((RESERVED_SECTORS + index * self.fat_sectors) * SECTOR_SIZE)
            image.write(self.fat)


def _allocate_tree(node: Node, layout: Fat32Layout, *, root: bool = False) -> None:
    if node.is_dir:
        # Entry byte length is independent of final cluster numbers.
        needed_bytes = len(_directory_bytes(node, root=root))
        count = max(1, math.ceil(needed_bytes / layout.cluster_size))
    else:
        count = math.ceil(node.size / layout.cluster_size)
    node.cluster_count = count
    node.first_cluster = layout.allocate(count)
    if root and node.first_cluster != ROOT_CLUSTER:
        raise AssertionError('FAT32 root was not placed at cluster 2')
    for child in node.children:
        _allocate_tree(child, layout)


def _write_tree(node: Node, layout: Fat32Layout, image: BinaryIO, *,
                root: bool = False, parent_cluster: int = ROOT_CLUSTER) -> None:
    if node.is_dir:
        data = _directory_bytes(node, root=root, parent_cluster=parent_cluster)
        image.seek(layout.cluster_offset(node.first_cluster))
        image.write(data)
        for child in node.children:
            _write_tree(child, layout, image, parent_cluster=node.first_cluster)
    elif node.size:
        image.seek(layout.cluster_offset(node.first_cluster))
        with node.path.open('rb') as source:
            remaining = node.size
            while remaining:
                chunk = source.read(min(1 << 20, remaining))
                if not chunk:
                    raise OSError(f'source file changed during import: {node.path}')
                image.write(chunk)
                remaining -= len(chunk)
        if node.path.stat().st_size != node.size:
            raise OSError(f'source file changed during import: {node.path}')


def create_fat32_image_from_directory(source_directory: str | Path,
                                      image_path: str | Path, *,
                                      capacity: str = '16G') -> Path:
    """Import a directory into a sparse FAT32 volume (no RAM-sized disk image).

    Destination is created transactionally with os.replace. No source files
    are modified, and a failed import will never leave a partial target image.
    """
    if capacity not in CAPACITIES:
        raise ValueError(f'unsupported SD capacity {capacity!r}; choose {list(CAPACITIES)}')
    source = Path(source_directory)
    if not source.is_dir():
        raise NotADirectoryError(source)
    dest = Path(image_path)
    if dest.resolve() == source.resolve() or source.resolve() in dest.resolve().parents:
        raise ValueError('SD image must not be created inside the imported directory')
    root = _scan(source)
    size_bytes = CAPACITIES[capacity]
    spc = {'256M': 4, '512M': 8, '16G': 64, '32G': 64}[capacity]
    layout = Fat32Layout(size_bytes, spc)
    _allocate_tree(root, layout, root=True)
    dest.parent.mkdir(parents=True, exist_ok=True)
    temp_path = None
    try:
        with tempfile.NamedTemporaryFile(prefix='.lexxos-fat32-', suffix='.img',
                                         dir=dest.parent, delete=False) as image:
            temp_path = Path(image.name)
            image.truncate(size_bytes)
            layout.write_metadata(image)
            _write_tree(root, layout, image, root=True)
            image.flush()
        os.replace(temp_path, dest)
    finally:
        if temp_path is not None:
            temp_path.unlink(missing_ok=True)
    return dest
