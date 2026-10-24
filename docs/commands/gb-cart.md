# `gb-cart`

test Game Boy cartridge mapper.

## Справка shell

```text
gb-cart - test Game Boy cartridge mapper

Opens a Game Boy ROM as a cartridge backend and performs small ROM bank mapping reads without loading the full image.

Usage:
  gb-cart [--bank number] <rom>

Arguments:
  -b, --bank <number> default=2 - switchable ROM bank to select for the mapper read test
  rom required - Game Boy ROM image path
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
