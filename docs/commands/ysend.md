# `ysend`

send a VFS file over YMODEM/CRC.

## Справка shell

```text
ysend - send a VFS file over YMODEM/CRC

Sends one VFS file to the raw UART/USB console using YMODEM/CRC.

Usage:
  ysend <path> [name]

Arguments:
  path required - VFS source file path
  name - optional transmitted file name

Runtime:
  launch: foreground-task
  stack: 8192 bytes
  priority: 1
  memory: internal-only
  interface: console
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
