# `mv`

rename a writable VFS file or directory.

## Справка shell

```text
mv - rename a writable VFS file or directory

Renames or moves a file or directory inside the same writable filesystem.

Usage:
  mv <source> <target>

Arguments:
  source required - readable VFS source file
  target required - writable VFS target file

Runtime:
  launch: foreground-task
  stack: 4096 bytes
  priority: 1
  memory: internal-only
  interface: console
  internal workspace: 256 bytes
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
