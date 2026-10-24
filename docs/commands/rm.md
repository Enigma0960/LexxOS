# `rm`

remove a writable VFS file.

## Справка shell

```text
rm - remove a writable VFS file

Removes writable VFS files. This is supported for /tmp files and regular files on writable FAT mounts.

Usage:
  rm <path>

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
