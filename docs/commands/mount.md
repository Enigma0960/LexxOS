# `mount`

show or create VFS mounts.

## Справка shell

```text
mount - show or create VFS mounts

Usage:
  mount [<source> <target> [fat]]

Arguments:
  source - block device or VFS source path
  target - VFS mount target path
  type - filesystem type, default: fat

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
