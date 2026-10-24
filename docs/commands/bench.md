# `bench`

run portable LexxOS system benchmark.

## Справка shell

```text
bench - run portable LexxOS system benchmark

Collects board/runtime settings, runs CPU, memory, scheduler, multitask and VFS synthetic tests, then prints and saves a table report.

Usage:
  bench [--iterations count] [--tasks count] [--out path] [--io-path path] [--no-save]

Arguments:
  -n, --iterations <count> default=200000 - synthetic benchmark iterations, default: 200000
  -t, --tasks <count> default=2 - worker task count for multitask benchmark, default: 2, max: 4
  -o, --out <path> default=/mnt/sd/apps/bench/system-benchmark.txt - report file path, default: timestamped file in /mnt/sd/bench
  --io-path <path> default=/tmp/bench-io.tmp - temporary file used by the VFS I/O benchmark, default: /tmp/bench-io.tmp
  --no-save - print the report without saving it to VFS

Runtime:
  launch: foreground-task
  stack: 5120 bytes
  priority: 1
  memory: internal-only
  interface: console
  internal workspace: 27016 bytes
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
