# `write`

write text to a writable VFS file.

## Справка shell

```text
write - write text to a writable VFS file

Creates or truncates a writable VFS file and writes text joined by spaces. If text is omitted, data is copied from standard input.

Usage:
  write <path> [text ...]

Arguments:
  path required - VFS file path
  text - text to write; extra arguments are joined with spaces

Runtime:
  launch: foreground-task
  stack: 4096 bytes
  priority: 1
  memory: internal-only
  interface: console
  internal workspace: 225 bytes
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
