# Документация LexxOS

## Условные обозначения в примерах

`/path/to/LexxOS`, `/path/to/esp-idf` и другие `/path/to/...` заменяйте путями
своих установок. `XXXX` в serial-портах заменяйте идентификатором устройства.
Имена `example.mid`, `test-rom-1.gb`, `test-rom-2.gb` и тестовых каталогов условные;
эти ресурсы не поставляются с ОС. Перед файловыми тестами убедитесь, что выбранные
имена свободны. Пути `/mnt/sd`, `/tmp`, `/proc` и `/sys` относятся к VFS LexxOS.
В исторических отчётах нейтральные имена заменяют локальные идентификаторы,
а результаты и параметры проверок сохраняются.

## Основы

- [Сборка](build.md)
- [Выпуск 0.1.0 — состав, установка и ограничения](release-0.1.0.md)
- [Сборка релизных архивов](release-packaging.md)
- [Результаты проверок 0.1.0](tests/release-0.1.0-validation.md)
- [Ручной тест-план 0.1.0](tests/release-0.1.0-manual.md)
- [Проверки разработки](development-checks.md)
- [Подготовка 0.1.0 и единый roadmap](../roadmap.md)
- [Архитектура](architecture.md)
- [Архитектурная дорожная карта](architecture-roadmap.md)
- [Runtime-профиль программ](program-runtime.md)
- [Аудит стеков программ](program-stack-audit.md)
- [История подготовки 0.0.7](release-0.0.7.md)
- [Профили плат](boards.md)
- [Правила структуры C++-кода](code-style.md)
- [Настройка CLion](clion.md)
- [Локальная сборка OpenOCD](openocd-local.md)
- [Python-эмулятор](../emulator/README.md)

## Файловая система и SD

- [FAT filesystem](fat-filesystem.md)
- [Backend FatFs](fatfs-backend.md)
- [Регрессионный тест-план FAT](fat-test-plan.md)
- [Production-профиль SD](sd-card-production.md)
- [Подключение card-detect](sd-card-detect.md)

## Устройства

- [RTC DS3231 и EEPROM AT24C32](rtc-ds3231-at24c32.md)
- [Контроллер Sega Genesis](sega-genesis-gamepad.md)
- [ClockworkPi PicoCalc / Pico, Pico W, Pico 2, Pico 2 W](boards/picocalc_pico_w.md)
- [PicoCalc southbridge](picocalc-southbridge.md)
- [PicoCalc JCS keyboard/southbridge firmware](picocalc-jcs-keyboard-firmware.md)
- [PicoCalc LCD-консоль](picocalc-lcd-console.md)
- [Game Boy emulator](gameboy.md)

## STM32F407

- [Первичный запуск](stm32f407-bringup.md)
- [Настройка macOS, CLion и Debug Probe](stm32f407-clion-macos.md)
- [Параллельный TFT-дисплей](stm32f407-display.md)
- [microSD через SDIO](stm32f407-sdio.md)
- [Проверка надёжности SDIO](stm32f407-sdio-reliability.md)

## Команды и планы

- [Индекс команд](commands/README.md)
- [Матрица команд и условий сборки](command-matrix.md)
- [Как документировать команды](command-docs.md)
- [Регрессионные проверки](tests/README.md)
- [Активный roadmap](../roadmap.md)
