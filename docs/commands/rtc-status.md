# `rtc-status`

## Исходник

`src/lexxos/apps/rtc/rtc_status_program.cpp`

## Подробное описание

Читает статус выбранного RTC backend: источник, raw status byte, oscillator-stop/voltage-low flags, software RTC
snapshot и дополнительные поля JCS firmware, когда они доступны.

## Параметры

- `--source <auto|software|picocalc|ds3231|pcf8563>` / `-s <...>` — выбрать RTC backend; по умолчанию `auto`.
- `--bus <name>` / `-b <name>` — I2C-шина для внешнего RTC; по умолчанию `debug`.
- `--address <addr>` / `-a <addr>` — 7-битный адрес внешнего RTC; если не задан, используется адрес backend-а.

## Примеры

```text
lexx:/> rtc-status
lexx:/> rtc-status --source software
lexx:/> rtc-status --source picocalc
lexx:/> rtc-status --source ds3231 --bus debug --address 0x68
lexx:/> rtc-status --source pcf8563 --bus system
```

## Комментарий автора

На PicoCalc JCS firmware `auto` выбирает southbridge RTC. DS3231 и PCF8563/BM8563 остаются отдельными I2C backend-ами.
Software RTC должен быть явно включен в профиле платы.

## Справка shell

```text
rtc-status - read RTC backend status

Reads status from the selected RTC backend.

Usage:
  rtc-status [--source auto|software|picocalc|ds3231|pcf8563] [--bus <name>] [--address <addr>]

Arguments:
  -s, --source <auto|software|picocalc|ds3231|pcf8563> default=auto - RTC backend source
  -b, --bus <name> default=debug - I2C bus name
  -a, --address <addr> - RTC 7-bit I2C address, decimal or hex; backend default when omitted
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
