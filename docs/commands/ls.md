# `ls`

list VFS directory.

## Справка shell

```text
ls - list VFS directory

Usage:
  ls [path]

Arguments:
  path - optional VFS path

Runtime:
  launch: foreground-task
  stack: 8192 bytes
  priority: 1
  memory: internal-only
  interface: console
  internal workspace: 128 bytes
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
