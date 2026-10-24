# `gamepad-live`

run a live gamepad input test.

## Справка shell

```text
gamepad-live - run a live gamepad input test

Polls a gamepad for a limited time and prints every state change.

Usage:
  gamepad-live [-s seconds] [-i milliseconds] [name]

Arguments:
  -s, --seconds <seconds> default=30 - test duration in seconds
  -i, --interval <milliseconds> default=25 - poll interval in milliseconds
  name - gamepad device name
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
