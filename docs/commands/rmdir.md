# `rmdir`

remove an empty writable VFS directory.

## Справка shell

```text
rmdir - remove an empty writable VFS directory

Removes an empty directory on writable filesystems. FAT supports short 8.3 names and long names when the backend is configured with LFN support.

Usage:
  rmdir <path>

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
