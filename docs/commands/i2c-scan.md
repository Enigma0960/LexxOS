# `i2c-scan`

## Исходник

`src/lexxos/apps/i2c_scan_program.cpp`

## Подробное описание

Сканирует зарегистрированную I2C-шину и печатает таблицу адресов в стиле `i2cdetect`. Команда полезна для первичного
запуска плат, проверки DS3231 и осторожной диагностики периферии.

## Параметры

- `--bus <name>` / `-b <name>` — необязательное имя I2C-шины; по умолчанию `debug`.
- `<bus-name>` — необязательный короткий позиционный alias для `--bus`.
- `--from <addr>` — первый 7-битный адрес; десятичный или `0xhex`; по умолчанию `0x08`.
- `--to <addr>` — последний 7-битный адрес; десятичный или `0xhex`; по умолчанию `0x77`.
- `--quiet` / `-q` — печатать найденные адреса простым списком.

## Примеры

```text
lexx:/> i2c-scan
lexx:/> i2c-scan system
lexx:/> i2c-scan --bus debug --from 0x08 --to 0x77
lexx:/> i2c-scan --quiet
```

## Комментарий автора

Не используйте `i2c-scan` для обхода владельца southbridge во время обычного ввода с PicoCalc keyboard. Для
безопасного состояния PicoCalc используйте `picocalc-info` и `picocalc-sb`.

## Справка shell

```text
i2c-scan - scan I2C bus

Scans a registered I2C bus and prints an i2cdetect-style address table to stdout. Use shell redirection to save the report to /tmp.

Usage:
  i2c-scan [<bus-name>] [--bus <name>] [--from <addr>] [--to <addr>] [--quiet]

Arguments:
  bus-name - optional I2C bus name positional alias
  -b, --bus <name> default=debug - I2C bus name
  --from <addr> default=0x08 - first 7-bit address, decimal or hex
  --to <addr> default=0x77 - last 7-bit address, decimal or hex
  -q, --quiet - print detected addresses as a plain list
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
