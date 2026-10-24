# `yrx`

receive a file over YMODEM/CRC.

## Справка shell

```text
yrx - receive a file over YMODEM/CRC

Receives one YMODEM/CRC file from the raw UART/USB console and writes it to VFS.

Usage:
  yrx [path]

Arguments:
  path - optional VFS destination path; when omitted, the YMODEM file name is used

Runtime:
  launch: foreground-task
  stack: 8192 bytes
  priority: 1
  memory: internal-only
  interface: console
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
