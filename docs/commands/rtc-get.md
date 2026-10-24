# `rtc-get`

## Исходник

`src/lexxos/apps/rtc/rtc_get_program.cpp`

## Подробное описание

Читает дату и время из выбранного RTC backend и печатает источник, значение RTC и weekday. Если oscillator-stop,
voltage-low или software-valid flag требует внимания, команда предупреждает, что время может быть недостоверным.

## Параметры

- `--source <auto|software|picocalc|ds3231|pcf8563>` / `-s <...>` — выбрать RTC backend; по умолчанию `auto`.
- `--bus <name>` / `-b <name>` — I2C-шина для внешнего RTC; по умолчанию `debug`.
- `--address <addr>` / `-a <addr>` — 7-битный адрес внешнего RTC; если не задан, используется адрес backend-а.

## Примеры

```text
lexx:/> rtc-get
lexx:/> rtc-get --source software
lexx:/> rtc-get --source picocalc
lexx:/> rtc-get --source pcf8563 --bus system
```

## Комментарий автора

Используйте `rtc-get` после boot и после `rtc-set`, чтобы проверить источник времени для PicoCalc status bar.

## Справка shell

```text
rtc-get - read RTC date and time

Reads current date/time from the selected RTC backend.

Usage:
  rtc-get [--source auto|software|picocalc|ds3231|pcf8563] [--bus <name>] [--address <addr>]

Arguments:
  -s, --source <auto|software|picocalc|ds3231|pcf8563> default=auto - RTC backend source
  -b, --bus <name> default=debug - I2C bus name
  -a, --address <addr> - RTC 7-bit I2C address, decimal or hex; backend default when omitted
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
