# `psram-info`

## Исходник

`src/lexxos/apps/picocalc/psram_info_program.cpp`

## Подробное описание

Печатает распиновку PSRAM из активного профиля платы и читает JEDEC ID командой `0x9F` через консервативный
программный SPI. На PicoCalc это диагностика первичного запуска внешней PSRAM без DMA и общих IRQ.

## Параметры

Команда не принимает пользовательских параметров.

## Примеры

```text
lexx:/> psram-info
psram: cs=20 sck=21 mosi=2 miso=3 sio2=4 sio3=5
jedec-id: 00 00 00 0D 5D 53 31 76
```

## Комментарий автора

Проверка на реальном ClockworkPi PicoCalc / Pico W подтвердила ответ `00 00 00 0D 5D 53 31 76`. Если все байты
равны `00` или `ff`, команда считает, что PSRAM не ответила.

## Справка shell

```text
psram-info - read external PSRAM JEDEC ID

Reads command 0x9F from the PSRAM SPI wiring declared by the active board profile.

Usage:
  psram-info
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
