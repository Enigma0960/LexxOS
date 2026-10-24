# `gb-info`

show Game Boy ROM cartridge header.

## Справка shell

```text
gb-info - show Game Boy ROM cartridge header

Reads the first 0x150 bytes of a Game Boy ROM image through VFS and prints cartridge metadata.

Usage:
  gb-info <rom>

Arguments:
  rom required - Game Boy ROM image path
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
