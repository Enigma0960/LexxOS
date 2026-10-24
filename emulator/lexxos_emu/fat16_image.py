from __future__ import annotations

import shutil
from pathlib import Path

from ._fat16_builder import (
    SECTOR_SIZE,
    Fat16Builder,
    allocate_tree,
    load_tree,
    write_tree,
)


def create_fat16_image_from_directory(
        source_directory: str | Path,
        image_path: str | Path,
        *,
        size_mib: int = 128,
        sectors_per_cluster: int = 8,
        root_entry_count: int = 512,
        max_file_size: int = 64 * 1024 * 1024,
) -> Path:
    """Create a deterministic FAT16 image containing a host directory."""
    source_path = Path(source_directory)
    if not source_path.exists():
        raise FileNotFoundError(source_path)
    if not source_path.is_dir():
        raise NotADirectoryError(source_path)

    total_sectors = (int(size_mib) * 1024 * 1024) // SECTOR_SIZE

    root = load_tree(source_path, max_file_size=max_file_size)
    if root is None:
        raise FileExistsError(source_path)

    builder = Fat16Builder(
        total_sectors=total_sectors,
        sectors_per_cluster=sectors_per_cluster,
        root_entry_count=root_entry_count,
    )

    root.first_cluster = 0
    for child in root.children:
        allocate_tree(builder, child, parent_cluster=0)

    builder.write_boot_sector()
    write_tree(builder, root, root=True)
    builder.write_fats()

    target = Path(image_path)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(builder.image)
    return target


def create_fat16_image(path: str | Path, *, readme_text: str = "Hello from LexxOS emulator tests\n") -> Path:
    """Create the small README-only image used by existing emulator tests."""
    target = Path(path)
    source = target.parent / f".{target.stem}-source"
    if source.exists():
        shutil.rmtree(source)
    source.mkdir(parents=True)
    try:
        (source / "README.TXT").write_text(readme_text, encoding="utf-8")
        return create_fat16_image_from_directory(source, target, size_mib=4, sectors_per_cluster=1)
    finally:
        shutil.rmtree(source, ignore_errors=True)


__all__ = ["create_fat16_image", "create_fat16_image_from_directory"]
