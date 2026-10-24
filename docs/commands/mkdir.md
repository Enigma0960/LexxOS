# `mkdir`

create a writable VFS directory.

## Справка shell

```text
mkdir - create a writable VFS directory

Creates a directory on writable filesystems. FAT supports short 8.3 names and ASCII long names.

Usage:
  mkdir <path>

Arguments:
  path required - required VFS path

Runtime:
  launch: foreground-task
  stack: 4096 bytes
  priority: 1
  memory: internal-only
  interface: console
  internal workspace: 128 bytes
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
