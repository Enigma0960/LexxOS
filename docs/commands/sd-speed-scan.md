# `sd-speed-scan`

find stable SD SPI baud rate.

## Справка shell

```text
sd-speed-scan - find stable SD SPI baud rate

Scans SD SPI transfer rates. Supports positional arguments and key=value aliases such as max=25000000 step=1000000 blocks=16 files=4.

Usage:
  sd-speed-scan <max-hz> <step-hz> [blocks-per-file] [files] [read|rw] [batch=1|2] [start=<block>] [dma=on|off] [fast-write=on|off]

Arguments:
  max-hz required - maximum SPI baud rate to test
  step-hz required - baud rate decrement between attempts
  blocks - optional 512-byte blocks per FAT file, default: 64, max: 1024
  files - optional FAT file count, default: 8, max: 64
  scope - optional probe scope: read-only raw scan or explicit raw+FAT write/read, default: read
  batch - optional raw read batch size; 1 uses CMD17, 2 uses CMD18
  start - optional first raw block for diagnostics
  dma - optional raw payload transfer mode; off forces blocking SPI instead of DMA
  fast-write - optional high-speed FAT write guard override for rw scope
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
