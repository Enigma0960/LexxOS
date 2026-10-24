# `poweroff`

## Исходник

`src/lexxos/apps/utility/poweroff_program.cpp`

## Подробное описание

Синхронизирует и размонтирует writable файловые системы, затем запрашивает выключение платы, если профиль устройства
поддерживает board-level power-off. На PicoCalc запрос идёт через `picocalc-southbridge` service.

## Параметры

Команда не принимает пользовательских параметров.

## Примеры

```text
lexx:/> poweroff
```

## Комментарий автора

На проверенной JCS-прошивке PicoCalc VFS sync/unmount проходит, но southbridge request может вернуть
`southbridge request failed`, и устройство продолжает работать. Это задокументированное ограничение 0.0.7.

## Справка shell

```text
poweroff - power off target device

Synchronizes mounted filesystems and requests board power-off when supported.

Usage:
  poweroff
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
