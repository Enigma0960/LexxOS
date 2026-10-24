# `picocalc-info`

Показывает информацию PicoCalc southbridge. `--direct-i2c` разрешает диагностическое чтение напрямую при отсутствии
зарегистрированного southbridge service.

## Использование

```text
picocalc-info [--direct-i2c] [--bus <name>] [--address <addr>]
```

## Наличие в сборке

Условие регистрации: семейство PicoCalc. Наличие команды не подтверждает подключение устройства
или поддержку всех операций конкретной конфигурацией. Подробности группировки:
[матрица команд](../command-matrix.md).

[Descriptor и реализация](../../src/lexxos/apps/picocalc/picocalc_info_program.cpp) — источник параметров;
`help picocalc-info` показывает справку текущей сборки.
