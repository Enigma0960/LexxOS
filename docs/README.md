# Документация LexxOS

Документация ведётся на русском. Имена API, параметры сборки, команды,
пути и дословный вывод программ сохраняются в исходном виде. Оригиналы
лицензий сторонних компонентов и необработанные UART-журналы не переводятся.

## С чего начать

- [Сборка и запуск](build.md), [настройка CLion](clion.md),
  [пакеты CMake для внешних проектов](cmake-packages.md).
- [Структура проекта](project-layout.md), [правила C++](code-style.md).
- [Архитектура](architecture.md), [среда выполнения программ](program-runtime.md),
  [профили памяти](runtime-memory-profiles.md).
- [Проверки разработки](development-checks.md), [каталог проверок](tests/README.md).
- [Профили плат](boards.md), [справочник команд](commands/README.md),
  [матрица команд](command-matrix.md).

## Архитектура и дальнейшая работа

Эти документы описывают разные уровни работы; предложения не означают,
что соответствующий API уже реализован.

- [Общая дорожная карта](../roadmap.md) — задачи и критерии выпуска.
- [Переход к новой архитектуре](architecture-roadmap.md) — выполненные шаги и остаток миграции.
- [Драйверы, составные платы и загрузка](hardware-boot-architecture-plan.md),
  [первый этап аппаратной миграции](hardware-migration-first-step.md).
- [План LexxCE](lexxce-plan.md) — предложение встраиваемой компоновки
  общего ядра для контроллеров и экранов. Начат первый срез CE-00/CE-01:
  общий `CoreSystem`, `CoreContext`, bootstrap и исправления ESP-IDF;
  готов [SDK для ESP32](../ce/README.md), аппаратная приёмка ещё впереди.
- [Пример CE blink для ESP-WROOM-32](../ce/examples/blink/README.md) — самостоятельная
  сборка, GPIO2 и UART-диагностика без shell.
- [SDK и графика](lexxos-sdk-graphics-roadmap-v2.md),
  [архитектура LexxUI](lexxui-system-architecture.md),
  [компоновка и элементы управления](lexxui-s52-layout-controls.md).
- [Аудит FreeRTOS от 31.10.2026](lexxos-freertos-kernel-audit-20261031.md),
  [IRQ/DMA и ядро ARMv7-A](armv7a-irq-dma-kernel-roadmap.md).

## Платы и устройства

- [PicoCalc / Luckfox Lyra M (RK3506G2)](boards/picocalc_lyra_m.md) — единое
  руководство: подключение, сборка, SD, MMU/DMA, отладка и история внедрения.
- [Порт загрузочного образа Lyra](../ports/lyra/README.md),
  [U-Boot PicoCalc Lyra](../ports/uboot/picocalc_lyra/README.md).
- [PicoCalc с Pico, Pico W, Pico 2 и Pico 2 W](boards/picocalc_pico_w.md).
- [JC-ESP32P4-M3-DEV](boards/jc_esp32p4_m3_dev.md), [порт ESP-IDF](../ports/espidf/README.md).
- [Сервис STM32 PicoCalc](picocalc-southbridge.md),
  [прошивка клавиатуры JCS](picocalc-jcs-keyboard-firmware.md),
  [раскладка](picocalc-keyboard-layout.md), [LCD-консоль](picocalc-lcd-console.md).
- [RTC DS3231 / EEPROM AT24C32](rtc-ds3231-at24c32.md),
  [геймпад Sega Genesis](sega-genesis-gamepad.md).
- STM32F407: [первичный запуск](stm32f407-bringup.md),
  [macOS/CLion/отладчик](stm32f407-clion-macos.md), [дисплей](stm32f407-display.md),
  [SDIO](stm32f407-sdio.md), [надёжность SDIO](stm32f407-sdio-reliability.md).
- [Локальная сборка OpenOCD](openocd-local.md).

## Приложения и инструменты

- [Приложения](../apps/README.md), [гостевой SDK](../apps/sdk/README.md),
  [диагностика SDK](../apps/sdk-demo/README.md).
- [Формат и запуск `.lexx`](external-programs-lexx.md),
  [манифест](external-programs-manifest.md), [аргументы](external-programs-arguments.md),
  [графический ABI](external-programs-graphics-abi.md).
- [Ввод](external-programs-input-model.md), [UART-ввод](external-programs-uart-input.md),
  [чтение файлов](external-programs-storage-read.md),
  [автозапуск](external-programs-autorun.md), [диагностика ошибок](external-programs-troubleshooting.md).
- [Эмулятор Game Boy](gameboy.md), [аудит стеков программ](program-stack-audit.md).
- [Python-эмулятор платформы](../tools/emulator/README.md),
  [передача файлов LexxCP](../tools/lexxcp/README.md).
- [Как документировать команды](command-docs.md).

## Файловая система и SD

- [Файловая система FAT](fat-filesystem.md), [реализация FatFs](fatfs-backend.md).
- [Регрессионный план FAT](fat-test-plan.md), [рабочий профиль SD](sd-card-production.md).
- [Обнаружение карты](sd-card-detect.md).

## Выпуски и история проверок

- [Подготовка 0.1.1](release-0.1.1.md).
- [Выпуск 0.1.0](release-0.1.0.md), [результаты](tests/release-0.1.0-validation.md),
  [ручной план](tests/release-0.1.0-manual.md).
- [История 0.0.7](release-0.0.7.md), [упаковка релизов](release-packaging.md).

Исторические отчёты фиксируют состояние на дату проверки. Они не заменяют
текущие инструкции и не подтверждают последующие изменения. Ранние заметки
`docs/boards/rk3506*` сведены в руководство Lyra; подробные прежние версии
доступны в истории Git. Новые сведения по плате добавляются в его разделы,
а новые датированные результаты испытаний — в `docs/tests/`.

## Обозначения в примерах

`/path/to/LexxOS`, `/path/to/esp-idf` и другие `/path/to/...` заменяйте своими
путями, `XXXX` в последовательных портах — идентификатором устройства.
`example.mid`, `test-rom-1.gb`, `test-rom-2.gb` и тестовые каталоги условны
и не поставляются с ОС. Перед файловыми тестами убедитесь, что имена свободны.
`/mnt/sd`, `/tmp`, `/proc`, `/sys` относятся к VFS LexxOS. В исторических
отчётах локальные идентификаторы могут быть заменены условными; измерения сохранены.
