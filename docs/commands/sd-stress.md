# `sd-stress`

run repeated FAT create/read/delete test.

## Справка shell

```text
sd-stress - run repeated FAT create/read/delete test

Creates several temporary files on /mnt/sd, verifies their contents and removes them repeatedly.

Usage:
  sd-stress [passes] [files]

Arguments:
  passes - optional pass count, default: 3, max: 20
  files - optional file count per pass, default: 4, max: 8

Runtime:
  launch: foreground-task
  stack: 5120 bytes
  priority: 1
  memory: internal-only
  interface: console
  internal workspace: 1155 bytes
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
