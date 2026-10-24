# `sd-bench`

benchmark SD raw read and FAT read/write.

## Справка shell

```text
sd-bench - benchmark SD raw read and FAT read/write

Reads raw SD blocks, writes a temporary FAT file, reads it back and verifies a generated pattern.

Usage:
  sd-bench [--delay-ms ms] [blocks] [path]

Arguments:
  blocks - optional 512-byte block count, default: 128, max: 1024
  path - optional FAT benchmark file path, default: /mnt/sd/LEXBENCH.TXT
  -d, --delay-ms <ms> default=0 - optional delay after each 512-byte block, default: 0, max: 1000

Runtime:
  launch: foreground-task
  stack: 5120 bytes
  priority: 1
  memory: internal-only
  interface: console
  internal workspace: 640 bytes
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
