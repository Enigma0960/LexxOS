# `gb-test-run`

run Game Boy hardware test ROM with serial capture.

## Справка shell

```text
gb-test-run - run Game Boy hardware test ROM with serial capture

Loads a Game Boy test ROM, runs it for a bounded cycle budget and prints the captured FF01/FF02 serial output used by many Blargg test ROMs.

Usage:
  gb-test-run [--cycles count] [--trace] <rom>

Arguments:
  -c, --cycles <count> default=2247168 - maximum emulated CPU cycles before stopping the test ROM
  -t, --trace - print final CPU stop diagnostics
  rom required - Game Boy test ROM image path
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
