# `cp`

copy readable VFS file to writable VFS file.

## Справка shell

```text
cp - copy readable VFS file to writable VFS file

Copies data from a readable VFS file to a writable VFS file using FileHandle read/write operations. Targets may be /tmp or a writable FAT mount.

Usage:
  cp <source> <target>

Arguments:
  source required - readable VFS source file
  target required - writable VFS target file

Runtime:
  launch: foreground-task
  stack: 4096 bytes
  priority: 1
  memory: internal-only
  interface: console
  internal workspace: 385 bytes
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
