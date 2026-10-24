# `touch-draw`

draw with one or more fingers.

## Справка shell

```text
touch-draw - draw with one or more fingers

Runs a full-screen touch drawing test with one colored stroke per active contact id.

Usage:
  touch-draw [name] [-d display] [-s seconds] [-r radius]

Arguments:
  name - touch-capable pointing device name
  -d, --display <name> - display device name
  -s, --seconds <seconds> default=60 - test duration
  -r, --radius <pixels> default=3 - brush radius
```

## Примеры

```text
lexx:/> touch-draw
lexx:/> touch-draw -s 30 -r 4
```

`touch-draw` использует отдельный цвет для каждого active contact id, поэтому подходит для проверки single-touch и
multi-touch.

## Доступность

Доступна в JC ESP32-P4 profile с touch-capable pointing device и display device. Документ соответствует LexxOS 0.0.7.
