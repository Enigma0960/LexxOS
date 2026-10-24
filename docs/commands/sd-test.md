# `sd-test`

run SD FAT read/write smoke test.

## Справка shell

```text
sd-test - run SD FAT read/write smoke test

Writes a small file to /mnt/sd, reads it back, compares contents and removes the test file.

Usage:
  sd-test [path]

Arguments:
  path - optional writable FAT test file path, default: /mnt/sd/LEXTEST.TXT

Runtime:
  launch: foreground-task
  stack: 4096 bytes
  priority: 1
  memory: internal-only
  interface: console
  internal workspace: 225 bytes
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
