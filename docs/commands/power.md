# `power`

## Исходник

`src/lexxos/apps/utility/power_program.cpp`

## Подробное описание

Показывает состояние питания платы, батареи и поддержки программного выключения. На PicoCalc команда читает
service-backed snapshot `picocalc-southbridge` без прямого I2C-доступа из shell.

## Параметры

Команда не принимает пользовательских параметров.

## Примеры

```text
lexx:/> power
power: source=picocalc-southbridge direct-i2c=no
```

## Комментарий автора

`power` должен оставаться read-only диагностикой. Для изменения подсветки используйте `backlight`, для режима дисплея
`display-power`, а для выключения устройства `poweroff`.

## Справка shell

```text
power - show board power status

Shows board power, battery and power-control state when supported.

Usage:
  power
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
