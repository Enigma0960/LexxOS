from __future__ import annotations

import math
import logging

from dataclasses import dataclass, field
from pathlib import Path

from ._fat16_names import (
    ARCHIVE_ATTRIBUTE,
    DIRECTORY_ATTRIBUTE,
    _write_le16,
    _write_le32,
    create_directory_entry,
    create_dot_entry,
    make_unique_short_alias,
)

SECTOR_SIZE = 512
FAT16_EOC = 0xFFFF


@dataclass
class FsNode:
    name: str
    source_path: Path
    is_dir: bool
    data: bytes = b""
    children: list["FsNode"] = field(default_factory=list)
    first_cluster: int = 0
    clusters: list[int] = field(default_factory=list)
    parent_cluster: int = 0

    @property
    def size(self) -> int:
        return 0 if self.is_dir else len(self.data)


def load_tree(path: Path, *, max_file_size: int) -> FsNode | None:
    if path.is_dir():
        children: list[FsNode] = []
        for child in sorted(path.iterdir(), key=lambda item: item.name.lower()):
            if child.is_symlink():
                continue
            if child.is_dir() or child.is_file():
                tree = load_tree(child, max_file_size=max_file_size)
                if tree is not None:
                    children.append(tree)
        return FsNode(name=path.name, source_path=path, is_dir=True, children=children)

    file_size = path.stat().st_size

    if file_size > max_file_size:
        logging.getLogger(__name__).warning(
            f"File {path} is too large: "
            f"{file_size / 1024 / 1024:.2f} MiB "
            f"(limit: {max_file_size / 1024 / 1024:.2f} MiB)"
        )

        return None

        # raise ValueError(
        #     f"File {path} is too large: "
        #     f"{file_size / 1024 / 1024:.2f} MiB "
        #     f"(limit: {max_file_size / 1024 / 1024:.2f} MiB)"
        # )

    data = path.read_bytes()
    return FsNode(name=path.name, source_path=path, is_dir=False, data=data)


class Fat16Builder:
    def __init__(self, *, total_sectors: int, sectors_per_cluster: int, root_entry_count: int):
        self.total_sectors = total_sectors
        self.sectors_per_cluster = sectors_per_cluster
        self.root_entry_count = root_entry_count
        self.reserved_sectors = 1
        self.fat_count = 2
        self.root_dir_sectors = (root_entry_count * 32 + SECTOR_SIZE - 1) // SECTOR_SIZE
        self.fat_size_sectors = self._choose_fat_size()
        self.root_dir_start = self.reserved_sectors + self.fat_count * self.fat_size_sectors
        self.data_start = self.root_dir_start + self.root_dir_sectors
        self.cluster_count = (self.total_sectors - self.data_start) // self.sectors_per_cluster
        if self.cluster_count < 4085:
            raise ValueError("generated image is too small for FAT16")
        if self.cluster_count > 65524:
            raise ValueError("generated image is too large for FAT16")
        self.fat: list[int] = [0] * (self.cluster_count + 2)
        self.fat[0] = 0xFFF8
        self.fat[1] = FAT16_EOC
        self.next_cluster = 2
        self.image = bytearray(self.total_sectors * SECTOR_SIZE)

    def _choose_fat_size(self) -> int:
        fat_size = 1
        while True:
            data_start = self.reserved_sectors + self.fat_count * fat_size + self.root_dir_sectors
            cluster_count = (self.total_sectors - data_start) // self.sectors_per_cluster
            needed = math.ceil((cluster_count + 2) * 2 / SECTOR_SIZE)
            if needed <= fat_size:
                return fat_size
            fat_size = needed

    def allocate_clusters(self, count: int) -> list[int]:
        if count <= 0:
            return []
        if self.next_cluster + count > len(self.fat):
            raise ValueError("host directory does not fit into generated FAT image")
        clusters = list(range(self.next_cluster, self.next_cluster + count))
        self.next_cluster += count
        for index, cluster in enumerate(clusters):
            self.fat[cluster] = clusters[index + 1] if index + 1 < len(clusters) else FAT16_EOC
        return clusters

    def cluster_offset(self, cluster: int) -> int:
        sector = self.data_start + (cluster - 2) * self.sectors_per_cluster
        return sector * SECTOR_SIZE

    @property
    def cluster_size(self) -> int:
        return self.sectors_per_cluster * SECTOR_SIZE

    def write_boot_sector(self) -> None:
        boot = bytearray(SECTOR_SIZE)
        boot[0:3] = b"\xEB\x3C\x90"
        boot[3:11] = b"MSDOS5.0"
        _write_le16(boot, 11, SECTOR_SIZE)
        boot[13] = self.sectors_per_cluster
        _write_le16(boot, 14, self.reserved_sectors)
        boot[16] = self.fat_count
        _write_le16(boot, 17, self.root_entry_count)
        if self.total_sectors <= 0xFFFF:
            _write_le16(boot, 19, self.total_sectors)
        else:
            _write_le16(boot, 19, 0)
            _write_le32(boot, 32, self.total_sectors)
        boot[21] = 0xF8
        _write_le16(boot, 22, self.fat_size_sectors)
        _write_le16(boot, 24, 63)
        _write_le16(boot, 26, 255)
        _write_le32(boot, 28, 0)
        boot[36] = 0x80
        boot[38] = 0x29
        _write_le32(boot, 39, 0x4C584F53)
        boot[43:54] = b"LEXXOS     "
        boot[54:62] = b"FAT16   "
        boot[510] = 0x55
        boot[511] = 0xAA
        self.image[0:SECTOR_SIZE] = boot

    def write_fats(self) -> None:
        fat_bytes = bytearray(self.fat_size_sectors * SECTOR_SIZE)
        for index, value in enumerate(self.fat):
            offset = index * 2
            if offset + 2 <= len(fat_bytes):
                _write_le16(fat_bytes, offset, value)
        for fat_index in range(self.fat_count):
            start = (self.reserved_sectors + fat_index * self.fat_size_sectors) * SECTOR_SIZE
            self.image[start:start + len(fat_bytes)] = fat_bytes

    def write_cluster_data(self, cluster: int, data: bytes) -> None:
        offset = self.cluster_offset(cluster)
        size = self.cluster_size
        self.image[offset:offset + size] = data.ljust(size, b"\x00")[:size]

    def write_file_data(self, node: FsNode) -> None:
        remaining = node.data
        for cluster in node.clusters:
            self.write_cluster_data(cluster, remaining[:self.cluster_size])
            remaining = remaining[self.cluster_size:]

    def write_directory_cluster(self, node: FsNode, directory_bytes: bytes) -> None:
        if not node.clusters:
            raise ValueError("directory has no allocated cluster")
        self.write_cluster_data(node.clusters[0], directory_bytes)

    def write_root_directory(self, directory_bytes: bytes) -> None:
        max_size = self.root_dir_sectors * SECTOR_SIZE
        if len(directory_bytes) > max_size:
            raise ValueError("root directory has too many entries for generated FAT image")
        start = self.root_dir_start * SECTOR_SIZE
        self.image[start:start + max_size] = directory_bytes.ljust(max_size, b"\x00")


def allocate_tree(builder: Fat16Builder, node: FsNode, *, parent_cluster: int = 0) -> None:
    node.parent_cluster = parent_cluster
    if node.is_dir:
        if parent_cluster != -1:
            node.clusters = builder.allocate_clusters(1)
            node.first_cluster = node.clusters[0]
        for child in node.children:
            allocate_tree(builder, child, parent_cluster=node.first_cluster)
    else:
        cluster_count = max(1, math.ceil(len(node.data) / builder.cluster_size)) if node.data else 0
        node.clusters = builder.allocate_clusters(cluster_count)
        node.first_cluster = node.clusters[0] if node.clusters else 0


def _directory_bytes(node: FsNode, *, root: bool = False) -> bytes:
    data = bytearray()
    if not root:
        data.extend(create_dot_entry(".", node.first_cluster))
        data.extend(create_dot_entry("..", node.parent_cluster))

    used_aliases: set[str] = set()
    for child in node.children:
        alias = make_unique_short_alias(child.name, used_aliases)
        attribute = DIRECTORY_ATTRIBUTE if child.is_dir else ARCHIVE_ATTRIBUTE
        data.extend(
            create_directory_entry(
                child.name,
                alias,
                attribute=attribute,
                first_cluster=child.first_cluster,
                size=child.size,
            )
        )
    return bytes(data)


def write_tree(builder: Fat16Builder, node: FsNode, *, root: bool = False) -> None:
    if node.is_dir:
        content = _directory_bytes(node, root=root)
        if root:
            builder.write_root_directory(content)
        else:
            if len(content) > builder.cluster_size:
                raise ValueError(f"directory has too many entries for one cluster: {node.source_path}")
            builder.write_directory_cluster(node, content)
        for child in node.children:
            write_tree(builder, child)
    else:
        builder.write_file_data(node)
