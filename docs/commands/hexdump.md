# `hexdump`

print file or stdin as hexadecimal bytes.

## Справка shell

```text
hexdump - print file or stdin as hexadecimal bytes

Displays file or standard input data as 16-byte hexadecimal rows with an ASCII representation.

Usage:
  hexdump [path]

Arguments:
  path - optional readable VFS file; stdin is used when omitted

Runtime:
  launch: foreground-task
  stack: 4096 bytes
  priority: 1
  memory: internal-only
  interface: console
  internal workspace: 145 bytes
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
