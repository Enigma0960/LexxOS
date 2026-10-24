# `gb-cpu-step`

step Game Boy CPU core.

## Справка shell

```text
gb-cpu-step - step Game Boy CPU core

Opens a Game Boy ROM, attaches it to the bus and executes a bounded number of SM83 CPU instructions for diagnostics.

Usage:
  gb-cpu-step [--steps count] [--pc address] <rom>

Arguments:
  -s, --steps <count> default=8 - number of CPU instructions to execute
  --pc <address> default=0x0100 - override initial program counter, decimal or 0x-prefixed hexadecimal
  rom required - Game Boy ROM image path
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
