# `fs-smoke`

run VFS writable filesystem smoke test.

## Справка shell

```text
fs-smoke - run VFS writable filesystem smoke test

Creates a temporary test directory and verifies long names, write, append, read, copy, rename, remove, rmdir and sync on a writable VFS mount.

Usage:
  fs-smoke [path]

Arguments:
  path - optional VFS path

Runtime:
  launch: foreground-task
  stack: 5120 bytes
  priority: 1
  memory: internal-only
  interface: console
  internal workspace: 793 bytes
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
