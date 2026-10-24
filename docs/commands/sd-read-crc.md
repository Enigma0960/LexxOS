# `sd-read-crc`

show or set SD read CRC validation.

## Справка shell

```text
sd-read-crc - show or set SD read CRC validation

Shows SD read CRC validation. In the stable build CRC validation stays off because the current SD cards/module proved more stable without this diagnostic path.

Usage:
  sd-read-crc [on|off]

Arguments:
  state - enable or disable CRC16 validation for SD read blocks
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
