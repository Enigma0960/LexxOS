# `sd-multiblock`

show or set SD CMD18 multi-block read usage.

## Справка shell

```text
sd-multiblock - show or set SD CMD18 multi-block read usage

Shows CMD18 multi-block read state. The default SPI baseline enables CMD18 for readBlocks(count > 1).

Usage:
  sd-multiblock [on|off]

Arguments:
  state - enable or disable CMD18 multi-block reads
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
