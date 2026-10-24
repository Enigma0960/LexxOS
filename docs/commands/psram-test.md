# `psram-test`

## Исходник

`src/lexxos/apps/picocalc/psram_test_program.cpp`

## Подробное описание

Выполняет осторожную проверку чтения и записи небольшого диапазона PSRAM: читает исходные байты, пишет два шаблона,
проверяет чтение и восстанавливает исходное содержимое.

## Параметры

- `--address <addr>` / `-a <addr>` — необязательный 24-битный начальный адрес; десятичный или `0xhex`; по умолчанию
  `0`.
- `--length <bytes>` / `-l <bytes>` — необязательная длина; по умолчанию `256`; диапазон `1..4096`.
- `--passes <count>` / `-p <count>` — количество проходов записи/чтения; по умолчанию `1`; диапазон `1..1000`.

## Примеры

```text
lexx:/> psram-test
psram-test: address=0x00000000 length=256
psram-test: ok restored=yes

lexx:/> psram-test --address 0x100 --length 128
psram-test: address=0x00000100 length=128
psram-test: ok restored=yes
```

## Комментарий автора

Диапазон намеренно ограничен 256 байтами, чтобы команда оставалась безопасной диагностикой до появления
распределителя памяти, пула framebuffer/cache или блочного представления PSRAM.

## Справка shell

```text
psram-test - test external PSRAM read/write

Reads, writes, verifies and restores a PSRAM block using the board PSRAM burst-transfer path.

Usage:
  psram-test [--address <addr>] [--length <bytes>] [--passes <count>]

Arguments:
  -a, --address <addr> default=0 - 24-bit start address, decimal or 0xhex
  -l, --length <bytes> default=256 - bytes to test, maximum 4096
  -p, --passes <count> default=1 - number of write/read/verify passes, maximum 1000
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
