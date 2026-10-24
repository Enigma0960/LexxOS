# SD-карта: production profile и статус этапа

Документ фиксирует текущую стабильную конфигурацию SD/FAT для LexxOS на Raspberry Pi Pico / RP2040.

Цель документа — отделить рабочий production-путь от экспериментальных направлений, чтобы в дальнейшем можно было
изменять Game Boy, display pipeline, VFS или SDIO-ветку без случайного отката стабильной SD-конфигурации.

## Production-конфигурация для `dev_pico`

Текущий рабочий профиль платы:

```text
LEXXOS_PLATFORM=pico
LEXXOS_BOARD=dev_pico
LEXXOS_SD_BACKEND=<empty> или spi
```

Автоматически выбирается:

```text
sd board interface: spi
sd interface: spi
```

Рабочие параметры SD SPI:

```text
init baud:        400000 Hz
safe baud:        1000000 Hz
default baud:     50000000 Hz
performance baud: 60000000 Hz
max test baud:    62000000 Hz
spi mode:         0
read crc:         off
CMD18 read:       on
CMD25 write:      off
DMA transfers:    on
high-speed writes: enabled
```

Рабочие параметры FAT/cache:

```text
LEXXOS_FAT_READ_CACHE_SECTORS=16
LEXXOS_FAT_WRITE_BUFFER_SECTORS=8
LEXXOS_FAT_DISK_WRITE_MAX_SECTORS=8
```

Runtime-профиль:

```text
sd-profile default
sd-multiwrite off
fat-cache write-max 8
```

`sd-multiwrite off` означает, что верхний FAT layer может отдавать запись пачками до 8 секторов, но SD SPI backend
раскладывает их в стабильные одиночные `CMD24` операции. Экспериментальный `CMD25` не используется в production path.

## Проверенные ориентиры производительности

Типичные результаты на текущей плате и SD-карте:

```text
raw CMD18 read at 60 MHz:     ~780-825 KiB/s
FAT sequential read/verify:   ~650-660 KiB/s
FAT sequential write:         ~145-150 KiB/s
sd-debug diagnostics:         stored=0 dropped=0 seq=0
```

`sd-bench 128` может иногда показывать запись около `120 KiB/s`. Для оценки стабильного режима важнее длинный тест на
несколько MiB, потому что короткий тест сильнее зависит от распределения FAT, состояния карты и служебных записей.

## Production-конфигурация для `picocalc_pico_w`

PicoCalc использует тот же SPI SD backend и FAT/VFS path, что `dev_pico`, но с board descriptor для ClockworkPi
PicoCalc:

```text
LEXXOS_PLATFORM=pico
LEXXOS_BOARD=picocalc_pico_w
LEXXOS_SD_BACKEND=<empty> или spi
```

Интерфейс:

```text
sd board interface: spi
sd interface: spi
SD SPI: GP18 SCK, GP19 MOSI, GP16 MISO, GP17 CS
card detect: GP22 active-low
stable production baud: 50000000 Hz
performance/read test ceiling: 60000000 Hz
```

Подтверждённый smoke на реальном PicoCalc:

```text
sd-test: readback ok, bytes=42
fs-smoke: ok
sd-bench: raw read 65536 bytes in 195 ms, 328 KiB/s
sd-bench: FAT write 65536 bytes in 493 ms, 129 KiB/s
sd-bench: FAT read/verify 65536 bytes in 356 ms, 179 KiB/s
sd-speed-scan 60000000 10000 1024: 60000000 Hz result=stable
```

Hot-plug/card-detect подтверждён:

```text
sd card: removed
sd automount: force-unmounted /mnt/sd (card removed, sync failed)
sd card: inserted
sd automount: probing (inserted)
sd automount: mounted /dev/sd0 on /mnt/sd type fat
```

PicoCalc отличается от `dev_pico` назначением интерфейсов платы: SPI0 используется под встроенный TF/SD slot PicoCalc,
SPI1 — под ILI9488 LCD. Поэтому PicoCalc SD speed настраивается отдельной CMake cache-переменной:

```text
LEXXOS_PICOCALC_SD_SPI_BAUDRATE=25000000
```

Пустое значение оставляет board default `50000000`.

## Обязательный regression checklist после изменений SD/FAT/VFS

Минимальный тест после изменений в SD backend, FAT cache, VFS write path или automount:

```text
storage
sd-profile
fat-cache
sd-bench 128
fat-cache
sd-debug
mem
```

Ожидаемые признаки:

```text
sd board interface: spi
sd interface: spi
active baud: 50000000 Hz
multi-block read: on
multi-block write: off
write max sectors: 8
write failures: 0
sd-debug: diagnostics=on stored=0 dropped=0 seq=0
```

`sd-speed-scan` остаётся одной shell-командой, но внутренняя реализация разделена на `apps/storage/detail` helper-модули:
argument parsing, raw-read probe, FAT write/read probe, reference-mode cleanup/recovery и report formatting. Основной
`sd_speed_scan_program.cpp` теперь отвечает только за порядок запуска проверки и выбор стабильной частоты.

Длинный RW-тест:

```text
fat-cache clear
sd-debug clear
sd-speed-scan max=60000000 step=1000000 blocks=256 files=16 scope=rw batch=2 dma=on fast-write=on
fat-cache
sd-debug
mem
```

Ожидаемые признаки:

```text
60000000 Hz result=stable
FAT write: примерно 145-150 KiB/s
FAT read/verify: примерно 650-660 KiB/s
write failures: 0
sd-debug: stored=0 dropped=0 seq=0
minimum ever free heap остаётся с запасом
```

Game Boy ROM regression:

```text
fat-cache clear
sd-debug clear
gb-rom-check /mnt/sd/gb/test-rom-1.gb
gb-cart /mnt/sd/gb/test-rom-1.gb
gb-cart /mnt/sd/gb/test-rom-2.gb
gbemu /mnt/sd/gb/test-rom-1.gb --frames 2
sd-debug
mem
```

Ожидаемые признаки:

```text
gb-rom-check: result=stable
gb-cart загружает MBC5/MBC1 ROM без ошибок
gbemu завершает bounded run без SD retries/failures
sd-debug: stored=0 dropped=0 seq=0
```

## Известное ограничение CMD25

`CMD25_WRITE_MULTIPLE_BLOCK` остаётся экспериментальным.

Во время тестов multi-block write через `CMD25` приводил к ошибке проверки статуса карты после записи:

```text
CMD13 status error
write fallback to safe baud
transfer failure
```

Ошибка воспроизводилась даже при небольшом `count=2`, поэтому production path намеренно оставляет:

```text
sd-multiwrite off
```

В этом режиме `writeBlocks(count > 1)` не использует `CMD25`, а выполняет последовательность стабильных `CMD24`.

Возвращаться к `CMD25` имеет смысл только отдельной задачей и только на raw scratch area, не через FAT. Для этого нужен
специальный тест, который:

- пишет во временную область карты вне файловой системы;
- проверяет data response token;
- корректно ждёт busy-release;
- проверяет `CMD13`;
- читает записанные блоки обратно и сравнивает payload;
- не трогает FAT volume.

## Экспериментальный RP2040 SDIO profile

Подготовлен профиль:

```text
LEXXOS_BOARD=dev_pico_sdio
LEXXOS_SD_BACKEND=sdio
LEXXOS_ENABLE_EXPERIMENTAL_RP2040_SDIO=ON
```

Он нужен как архитектурная заготовка и не является production-путём.

Предварительная SDIO-распиновка:

```text
DAT0 -> GPIO16
DAT1 -> GPIO17
DAT2 -> GPIO18
DAT3 -> GPIO19
CMD  -> GPIO20
CLK  -> GPIO21
CD   -> GPIO22
```

Эта распиновка конфликтует с текущими SPI SD и I2C debug OLED pins:

```text
SPI SD:    GPIO16..GPIO19
I2C OLED:  GPIO20..GPIO21
SDIO:      GPIO16..GPIO21
```

Поэтому SDIO оформлен как отдельный board profile, а не как переключатель внутри `dev_pico`.

До появления реальной PIO/SDIO реализации на RP2040 этот профиль следует считать guarded skeleton. Обычная сборка
`debug-pico-full` должна оставаться на `dev_pico` и SPI backend.

## Архитектурное состояние SD layer

Текущая структура SD backend:

```text
SdCardBackend
  ├── SdSpiBackend
  │     ├── SdSpiTransport
  │     ├── SdSpiProtocol
  │     └── SdSpiBlockOps
  ├── SdSdioBackend
  │     ├── STM32F407 SDIO implementation
  │     └── RP2040/host guarded skeleton
  └── SimSdCardBlockDevice
```

Основной application/VFS/FAT layer должен работать через `SdCardBackend` или `BlockDevice`, не через конкретный
`SdSpiBackend`.

## Критерии закрытия текущего SD-этапа

Этап можно считать закрытым, если после последнего патча проходят:

```text
storage
sd-bench 128
sd-speed-scan max=60000000 step=1000000 blocks=256 files=16 scope=rw batch=2 dma=on fast-write=on
gb-rom-check /mnt/sd/gb/test-rom-1.gb
gb-cart /mnt/sd/gb/test-rom-1.gb
gb-cart /mnt/sd/gb/test-rom-2.gb
gbemu /mnt/sd/gb/test-rom-1.gb --frames 2
```

И выполняются условия:

```text
sd-debug stored=0 dropped=0 seq=0
fat-cache write failures=0
FAT read/verify около 650 KiB/s
FAT write около 145-150 KiB/s на длинном тесте
heap не деградирует после повторных тестов
```

После этого дальнейшая работа с SD должна идти только как отдельная задача:

- исправление `CMD25`;
- реальная RP2040 PIO/SDIO реализация;
- дополнительные режимы cache/write-back;
- перенос на новые платы.

Для текущего этапа проекта SD/FAT можно считать production-ready и возвращаться к Game Boy/display pipeline.
