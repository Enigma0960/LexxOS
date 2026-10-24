# `touch`

create an empty writable VFS file.

## Справка shell

```text
touch - create an empty writable VFS file

Creates an empty file if it does not exist. Existing files are not truncated.

Usage:
  touch <path>

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
