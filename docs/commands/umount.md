# `umount`

unmount a VFS mount point.

## Справка shell

```text
umount - unmount a VFS mount point

Usage:
  umount <target>

Arguments:
  target required - VFS mount target path

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
