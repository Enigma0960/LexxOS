# `picocalc-sb`

## Исходник

`src/lexxos/apps/picocalc/picocalc_southbridge_program.cpp`

## Подробное описание

Показывает состояние PicoCalc southbridge через зарегистрированный сервис `picocalc-southbridge`. В обычной
PicoCalc-сборке не читает FIFO клавиатуры напрямую и не выполняет прямую запись, пока клавиатура владеет southbridge.

## Параметры

- `--direct-i2c` — необязательный диагностический флаг; разрешён только без зарегистрированного сервиса southbridge.
- `--bus <name>` / `-b <name>` — необязательное имя I2C-шины для прямого режима; по умолчанию `debug`.
- `--address <addr>` / `-a <addr>` — необязательный 7-битный I2C-адрес для прямого режима; по умолчанию `0x1F`.
- `--poll <count>` / `-p <count>` — устаревший опрос FIFO клавиатуры; пропускается при активной клавиатуре.
- `--delay <ms>` / `-d <ms>` — устаревшая задержка для старого пути опроса.
- `--lcd <0..255>`, `--kbd <0..255>`, `--reset <seconds>` — прямые записи; отклоняются при активном владельце
  клавиатуры.

## Примеры

```text
lexx:/> picocalc-sb
lexx:/> picocalc-sb --poll 2
lexx:/> picocalc-sb --direct-i2c
```

## Комментарий автора

Главный инвариант: не обходить `picocalc-keyboard` прямым I2C-чтением FIFO. Для обычной работы используйте
`backlight`, `display-power`, `power` и `poweroff`.

## Справка shell

```text
picocalc-sb - diagnose PicoCalc southbridge over I2C

Shows PicoCalc STM32 southbridge status through the registered service, with explicit direct-I2C diagnostics for bring-up.

Usage:
  picocalc-sb [--direct-i2c] [--bus <name>] [--address <addr>] [--poll <count>] [--delay <ms>] [--lcd <0..255>] [--kbd <0..255>] [--reset <seconds>]

Arguments:
  -b, --bus <name> default=debug - I2C bus name
  -a, --address <addr> default=0x1F - southbridge 7-bit I2C address
  -p, --poll <count> default=0 - deprecated keyboard FIFO reads; skipped while picocalc-keyboard owns input
  -d, --delay <ms> default=50 - delay between deprecated keyboard FIFO reads
  --lcd <0..255> - set LCD backlight brightness
  --kbd <0..255> - set keyboard backlight brightness
  --reset <seconds> - ask PicoCalc southbridge to reset the device after a delay
  --direct-i2c - diagnostic-only direct I2C access when no southbridge service is registered
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
