# `sd-multiwrite`

show or set experimental SD CMD25 multi-block writes.

## Справка shell

```text
sd-multiwrite - show or set experimental SD CMD25 multi-block writes

Shows or changes CMD25 multi-block write usage. Keep this off for the stable FAT path; use it only for isolated diagnostics.

Usage:
  sd-multiwrite [on|off]

Arguments:
  state - enable or disable experimental CMD25 multi-block writes
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
