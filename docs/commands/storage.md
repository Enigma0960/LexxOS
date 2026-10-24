# `storage`

show, probe, read or diagnose block storage.

## Справка shell

```text
storage - show, probe, read or diagnose block storage

Shows SD card information, probes storage, reads block zero/any block or runs a short diagnostic.

Usage:
  storage [probe|read0|read|diag] [block]

Arguments:
  action - optional action: probe, read0, read or diag
  block - block number for storage read

Runtime:
  launch: foreground-task
  stack: 4096 bytes
  priority: 1
  memory: internal-only
  interface: console
  internal workspace: 513 bytes
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
