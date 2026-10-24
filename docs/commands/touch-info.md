# `touch-info`

show touch panel state.

## Справка shell

```text
touch-info - show touch panel state

Prints touch-capable pointing device contacts, raw coordinates and calibration.

Usage:
  touch-info [name]

Arguments:
  name - touch-capable pointing device name
```

## Пример

```text
lexx:/> touch-info
device: ft5x06-touch connected=yes contacts=0/5 reports=1 last=10238ms
calibration: valid=yes swap-xy=no invert-x=no invert-y=no raw-x=0..799 raw-y=0..479 display=800x480
path: /mnt/sd/.lexx/touch.conf
```

## Доступность

Доступна в JC ESP32-P4 profile с touch-capable pointing device. Документ соответствует LexxOS 0.0.7.
