# `keyboard-raw`

run a live raw matrix keyboard test.

## Справка shell

```text
keyboard-raw - run a live raw matrix keyboard test

Polls an input device and prints raw matrix masks with row/column coordinates.

Usage:
  keyboard-raw [-s seconds] [-i milliseconds] [name]

Arguments:
  -s, --seconds <seconds> default=30 - test duration in seconds
  -i, --interval <milliseconds> default=25 - poll interval in milliseconds
  name - input device name
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
