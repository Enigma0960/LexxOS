# `gb-bus`

test Game Boy memory bus.

## Справка shell

```text
gb-bus - test Game Boy memory bus

Opens a Game Boy ROM as a cartridge, attaches it to a 64 KiB bus model and tests ROM, VRAM, WRAM, echo RAM, OAM, IO, HRAM and IE mapping.

Usage:
  gb-bus [--bank number] <rom>

Arguments:
  -b, --bank <number> default=2 - switchable ROM bank to select through the bus mapper control registers
  rom required - Game Boy ROM image path
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
