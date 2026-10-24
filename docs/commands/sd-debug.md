# `sd-debug`

show or control SD driver diagnostics.

## Справка shell

```text
sd-debug - show or control SD driver diagnostics

Shows the SD SPI diagnostic ring buffer and can enable, disable or clear recorded driver errors.

Usage:
  sd-debug [status|on|off|clear]

Arguments:
  action - optional diagnostic action; default: status
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
