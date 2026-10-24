# `display-power`

## Исходник

`src/lexxos/apps/devices/display_power_program.cpp`

## Подробное описание

Показывает или меняет режим питания display controller. На PicoCalc поверх режима дисплея применяется board policy:
`off`/`sleep` гасят LCD и keyboard backlight, а `on` восстанавливает последние уровни подсветки. На ESP32-P4 DSI
дисплеях команда использует device-level power/backlight API.

## Параметры

- Без параметров — показать режим основного дисплея.
- `on|off|sleep [name]` — переключить основной или указанный display device.
- `timeout` — показать текущую idle-timeout настройку.
- `timeout <seconds|off> [off|sleep]` — сохранить таймаут бездействия; диапазон секунд `0..86400`.

## Примеры

```text
lexx:/> display-power
lexx:/> display-power off
lexx:/> display-power on
lexx:/> display-power timeout 60 sleep
```

## Комментарий автора

Первый `KeyDown` после `off`/`sleep` будит экран и не передаётся дальше в shell. Настройка таймаута сохраняется в
`/mnt/sd/.lexx/display-power.conf`; если файла ещё нет, используется runtime default платы.

## Справка shell

```text
display-power - show or set display power mode

Shows or switches display controller power mode. Without a name it uses the primary display.

Usage:
  display-power [on|off|sleep|timeout|name] [name|seconds|off] [off|sleep]

Arguments:
  action - power mode, timeout action or display device name
  value - display device name, timeout seconds or timeout off
  extra - idle timeout display mode
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
