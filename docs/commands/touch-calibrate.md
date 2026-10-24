# `touch-calibrate`

calibrate touch panel.

## Справка shell

```text
touch-calibrate - calibrate touch panel

Captures screen targets, applies touch orientation/range calibration and saves it to SD.

Usage:
  touch-calibrate [restore|show|calibrate] [name] [-d display] [-n samples] [-t seconds] [--no-save]

Arguments:
  restore|show|calibrate - calibration action
  name - touch-capable pointing device name
  -d, --display <name> - display device name
  -n, --samples <count> default=8 - samples averaged at each calibration target
  -t, --timeout <seconds> default=20 - timeout for each target
  --no-save - apply calibration without saving it to SD
```

## Конфигурация

Профиль сохраняется на SD:

```text
/mnt/sd/.lexx/touch.conf
```

Если файл существует, LexxOS применяет его после mount `/mnt/sd` во время загрузки.

## Примеры

```text
lexx:/> touch-calibrate
lexx:/> touch-calibrate show
lexx:/> touch-calibrate restore
lexx:/> touch-calibrate --no-save
```

## Доступность

Доступна в JC ESP32-P4 profile с touch-capable pointing device и display device. Документ соответствует LexxOS 0.0.7.
