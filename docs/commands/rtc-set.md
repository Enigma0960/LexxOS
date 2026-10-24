# `rtc-set`

## Исходник

`src/lexxos/apps/rtc/rtc_set_program.cpp`

## Подробное описание

Записывает дату и время в выбранный RTC backend. Общий диапазон LexxOS — `2000-01-01..2199-12-31`; PicoCalc JCS
RTC поддерживает `2000..2099`, software RTC хранит время в RAM до выключения питания и использует Unix range.

## Параметры

- `--source <auto|software|picocalc|ds3231|pcf8563>` / `-s <...>` — выбрать RTC backend; по умолчанию `auto`.
- `--bus <name>` / `-b <name>` — I2C-шина для внешнего RTC; по умолчанию `debug`.
- `--address <addr>` / `-a <addr>` — 7-битный адрес внешнего RTC; если не задан, используется адрес backend-а.
- `<YYYY-MM-DD> <HH:MM:SS>` — обязательные дата и время; также допускается ISO-like первый аргумент
  `YYYY-MM-DDTHH:MM:SS`.

## Примеры

```text
lexx:/> rtc-set 2026-06-23 14:30:00
lexx:/> rtc-set --source software 2026-06-23 14:30:00
lexx:/> rtc-set --source picocalc 2026-06-23T14:30:00
lexx:/> rtc-set --source pcf8563 --bus system 2026-06-23 14:30:00
```

## Комментарий автора

После записи сразу проверьте `rtc-get` и `console-info`: status bar должен увидеть обновлённое время через тот же
RTC/southbridge путь.

## Справка shell

```text
rtc-set - set RTC date and time

Writes date/time to the selected RTC backend. Supported range is 2000-01-01 through 2199-12-31; PicoCalc JCS RTC supports 2000-2099, software RTC supports the 32-bit Unix time range.

Usage:
  rtc-set [--source auto|software|picocalc|ds3231|pcf8563] [--bus <name>] [--address <addr>] <YYYY-MM-DD> <HH:MM:SS>

Arguments:
  -s, --source <auto|software|picocalc|ds3231|pcf8563> default=auto - RTC backend source
  -b, --bus <name> default=debug - I2C bus name
  -a, --address <addr> - RTC 7-bit I2C address, decimal or hex; backend default when omitted
  date required - date or ISO-like date/time
  time - time when it is not included in the first argument
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
