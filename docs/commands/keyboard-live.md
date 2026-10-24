# `keyboard-live`

run a live keyboard input test.

## Справка shell

```text
keyboard-live - run a live keyboard input test

Polls an input device for a limited time and prints key events and pressed key changes.

Usage:
  keyboard-live [-s seconds] [-i milliseconds] [-f|--full] [name]

Arguments:
  -s, --seconds <seconds> default=30 - test duration in seconds
  -i, --interval <milliseconds> default=25 - poll interval in milliseconds
  -f, --full - also report unmapped raw events
  name - input device name
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
