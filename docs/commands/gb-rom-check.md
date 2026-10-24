# `gb-rom-check`

verify repeated Game Boy ROM reads.

## Справка shell

```text
gb-rom-check - verify repeated Game Boy ROM reads

Reads stable probe blocks from a ROM file and optionally hashes the full image. This helps separate CPU/PPU/display issues from SD/FAT/ROM-read instability.

Usage:
  gb-rom-check [--passes count] [--full] <rom>

Arguments:
  -p, --passes <count> default=2 - number of repeated file-read passes
  --full - also hash the full ROM image; slow on FAT/SD because it walks large files
  rom required - Game Boy ROM image path
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
