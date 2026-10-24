# `append`

append text to a writable VFS file.

## Справка shell

```text
append - append text to a writable VFS file

Creates a writable VFS file if needed and appends text joined by spaces. If text is omitted, data is copied from standard input.

Usage:
  append <path> [text ...]

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
