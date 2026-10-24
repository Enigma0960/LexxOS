# Матрица команд и условий сборки

Каталог [cmake/commands.json](../cmake/commands.json) управляет регистрацией и исходниками встроенных команд.
Команда включается при `LEXXOS_COMMANDS_<группа>=ON` **и** выполнении всех её зависимостей.
Группы не отключают драйверы и системные сервисы, которые могут использоваться ОС или другими командами.
`shell` — обязательный bootstrap runtime и остаётся независимо от группы SYSTEM.
При SYSTEM=OFF отсутствуют также help/exit; desktop-процесс завершается внешним сигналом.

## Группы и defaults

| Группа       | Назначение                                            | minimal / standard                                             | full                                                          |
|--------------|-------------------------------------------------------|----------------------------------------------------------------|---------------------------------------------------------------|
| SYSTEM       | Основное управление ОС                                | ON                                                             | ON                                                            |
| UTILITIES    | Файлы, терминал, повседневные утилиты                 | ON                                                             | ON                                                            |
| ADMIN        | Настройка, устройства, обновления                     | ON                                                             | ON                                                            |
| DIAGNOSTICS  | Наблюдение состояния                                  | ON                                                             | ON                                                            |
| APPLICATIONS | Редактор, VM, эмулятор, проигрыватель                 | ON, по feature-флагам                                          | ON, по feature-флагам                                         |
| TESTS        | Активные пробы, benchmark, стресс, отладочные команды | Начальное значение `LEXXOS_FEATURE_TEST_COMMANDS` (обычно OFF) | Начальное значение `LEXXOS_FEATURE_TEST_COMMANDS` (обычно ON) |
| EXPERIMENTAL | Экспериментальные инструменты, сейчас GPIO analyzer   | OFF                                                            | ON, только поддерживаемые конфигурации                        |

Каждая группа имеет отдельный BOOL-параметр `LEXXOS_COMMANDS_<имя>`. Для повторно используемого
build directory указывайте требуемые значения явно: изменение flavor не сбрасывает CMake cache.
`LEXXOS_COMMANDS_TESTS=OFF` перекрывает старые ON у трёх аппаратных test-флагов; при первой конфигурации TESTS задаёт
defaults всех трёх подгрупп. Их cache-значения
сохраняются для последующего включения, но effective feature header и каталог используют OFF.
`BUILD_TESTING` управляет CTest, а не тестовыми командами прошивки.

## Зависимости

Короткие feature-имена означают `LEXXOS_FEATURE_<имя>`. `SD_CARD_TEST_COMMANDS`,
`DISPLAY_TEST_COMMANDS`, `KEYBOARD_TEST_COMMANDS` дополнительно требуют TESTS и свой hardware feature.
При отключении DISPLAY автоматически исключаются display/touch-команды и GPIO analyzer.
Экран не нужен для VFS, UART-редактора и ANSI-утилит.

Остальные ключи вычисляет [генератор каталога](../tools/generate_command_catalog.py) по текущим board providers:

| Ключ                           | Значение                                                            |
|--------------------------------|---------------------------------------------------------------------|
| PICO / ESPIDF / DESKTOP / FULL | Платформа Pico / ESP-IDF / host или sim / flavor full               |
| PICOCALC / JC / CARDPUTER_ADV  | Семейство PicoCalc / jc_esp32p4_m3_dev / m5stack_cardputer_adv      |
| NET                            | ESP-IDF либо Pico с доступным CYW43 LwIP target                     |
| WIFI                           | ESP-IDF с native Wi-Fi либо JC с HOSTED; Pico с CYW43 LwIP          |
| HOSTED                         | `LEXXOS_ESPIDF_ENABLE_HOSTED`                                       |
| SD_SPI                         | Выбранный SD backend — spi                                          |
| ESP_SPI                        | ESP-IDF с платой M5Stack; JC использует другой набор шин            |
| LCD_CONSOLE                    | DISPLAY и PicoCalc/Cardputer/JC                                     |
| BACKLIGHT                      | DISPLAY и PicoCalc/ESP-IDF                                          |
| AUDIO                          | sim, PicoCalc, JC либо Cardputer Adv: соответствующий audio backend |
| MIC                            | JC либо Cardputer Adv: ES8311 capture backend                       |
| PORTABLE_BEEP                  | sim или ESP-IDF; у PicoCalc отдельная реализация beep               |
| LED                            | dev_pico/dev_pico_sdio/dev_pico2w, STM32 EUSE или sim               |
| I2C                            | Платформа кроме host                                                |
| GB_RUNTIME                     | PSRAM либо heap runtime host/sim/ESP-IDF                            |

На host нет физических display/input providers: соответствующие команды исключены даже при ON feature-флагах.
На PicoCalc Pico W сохраняется запрет тяжёлых GAMEBOY/MIDI/GPIO_ANALYZER без экспериментального override.
MIDI остаётся приложением, включаемым feature MIDI; EXPERIMENTAL сейчас управляет инструментом GPIO analyzer.
Сборка может гарантировать выбор backend, но runtime-проверки подключённой периферии и ошибок остаются нужны.

## Примеры

```sh
# Полная ОС без активных тестовых команд.
cmake --preset full-dev-sim -B build/no-command-tests -DLEXXOS_COMMANDS_TESTS=OFF
cmake --build build/no-command-tests
# Тестовые команды в новом minimal build directory; defaults подгрупп следуют TESTS.
cmake --preset minimal-dev-sim -B build/with-command-tests -DLEXXOS_COMMANDS_TESTS=ON -DLEXXOS_FEATURE_SD_CARD_TEST_COMMANDS=ON
# Без экрана: его тестовые подгруппы и analyzer тоже исключаются.
cmake --preset full-dev-sim -B build/headless -DLEXXOS_FEATURE_DISPLAY=OFF -DLEXXOS_FEATURE_DEBUG_DISPLAY=OFF
```

Фактический выбор и причины исключения: `<build>/generated/include/commands/commands.json`.
В этом же каталоге находятся `command_sources.cmake`, `command_selection.h`, `builtin_commands.inc`.
`help` показывает зарегистрированные команды; отключённые отдельные обработчики исключаются из source list,
а обработчики в общих touch/mouse единицах — препроцессором. Общие сервисы не удаляются вместе с группой.

## Система и shell

| Команда / документация                           | Операция                 | Уровень   | Зависимости каталога       | Группа        | Условия работы / примечания              |
|--------------------------------------------------|--------------------------|-----------|----------------------------|---------------|------------------------------------------|
| [`help`](commands/help.md)                       | справка / запуск         | ОС        | без аппаратных ограничений | `SYSTEM`      | ОС/shell                                 |
| [`board`](commands/board.md)                     | справка / запуск         | ОС        | без аппаратных ограничений | `SYSTEM`      | ОС/shell                                 |
| [`version`](commands/version.md)                 | справка / запуск         | ОС        | без аппаратных ограничений | `SYSTEM`      | ОС/shell                                 |
| [`uptime`](commands/uptime.md)                   | справка / запуск         | ОС        | без аппаратных ограничений | `SYSTEM`      | ОС/shell                                 |
| [`shell`](commands/shell.md)                     | справка / запуск         | ОС        | bootstrap, всегда          | `SYSTEM`      | ОС/shell                                 |
| [`jobs`](commands/jobs.md)                       | управление программами   | ОС        | без аппаратных ограничений | `SYSTEM`      | program manager                          |
| [`run`](commands/run.md)                         | управление программами   | ОС        | без аппаратных ограничений | `SYSTEM`      | program manager                          |
| [`stop`](commands/stop.md)                       | управление программами   | ОС        | без аппаратных ограничений | `SYSTEM`      | program manager                          |
| [`mem`](commands/mem.md)                         | наблюдение               | ОС        | без аппаратных ограничений | `DIAGNOSTICS` | runtime statistics                       |
| [`tasks`](commands/tasks.md)                     | наблюдение               | ОС        | без аппаратных ограничений | `DIAGNOSTICS` | runtime statistics                       |
| [`top`](commands/top.md)                         | наблюдение               | ОС        | без аппаратных ограничений | `DIAGNOSTICS` | runtime statistics                       |
| [`bench`](commands/bench.md)                     | benchmark                | ОС        | без аппаратных ограничений | `TESTS`       | benchmark backend                        |
| [`echo`](commands/echo.md)                       | вывод в терминал         | Сервис    | без аппаратных ограничений | `UTILITIES`   | терминал; физический экран не нужен      |
| [`clear`](commands/clear.md)                     | вывод в терминал         | Сервис    | без аппаратных ограничений | `UTILITIES`   | терминал; физический экран не нужен      |
| [`shell-config`](commands/shell-config.md)       | настройка                | Сервис    | без аппаратных ограничений | `ADMIN`       | shell; VFS для сохранения                |
| [`console-palette`](commands/console-palette.md) | проверка ANSI цветов     | Сервис    | без аппаратных ограничений | `TESTS`       | ANSI терминал; физический экран не нужен |
| [`reboot`](commands/reboot.md)                   | питание / перезапуск     | Платформа | без аппаратных ограничений | `SYSTEM`      | соответствующий platform service         |
| [`poweroff`](commands/poweroff.md)               | питание / перезапуск     | Платформа | без аппаратных ограничений | `SYSTEM`      | соответствующий platform service         |
| [`power`](commands/power.md)                     | питание / состояние      | Платформа | без аппаратных ограничений | `ADMIN`       | power service                            |
| [`bootsel`](commands/bootsel.md)                 | вход в загрузчик         | Платформа | PICO                       | `ADMIN`       | Pico USB boot service                    |
| [`exit`](commands/exit.md)                       | завершение симуляции     | Платформа | DESKTOP                    | `SYSTEM`      | host или sim                             |
| [`esp32-info`](commands/esp32-info.md)           | информация SDK/chip/heap | Платформа | ESPIDF                     | `DIAGNOSTICS` | ESP-IDF                                  |
| [`esp32-spi-test`](commands/esp32-spi-test.md)   | SPI smoke                | Драйвер   | ESPIDF ∧ ESP_SPI           | `TESTS`       | ESP-IDF и подходящая SPI-шина            |

## VFS и файлы

| Команда / документация             | Операция                  | Уровень    | Зависимости каталога       | Группа         | Условия работы / примечания                     |
|------------------------------------|---------------------------|------------|----------------------------|----------------|-------------------------------------------------|
| [`pwd`](commands/pwd.md)           | навигация / чтение        | Сервис     | без аппаратных ограничений | `UTILITIES`    | VFS; SD не обязательна                          |
| [`cd`](commands/cd.md)             | навигация / чтение        | Сервис     | без аппаратных ограничений | `UTILITIES`    | VFS; SD не обязательна                          |
| [`ls`](commands/ls.md)             | навигация / чтение        | Сервис     | без аппаратных ограничений | `UTILITIES`    | VFS; SD не обязательна                          |
| [`stat`](commands/stat.md)         | навигация / чтение        | Сервис     | без аппаратных ограничений | `UTILITIES`    | VFS; SD не обязательна                          |
| [`info`](commands/info.md)         | навигация / чтение        | Сервис     | без аппаратных ограничений | `UTILITIES`    | VFS; SD не обязательна                          |
| [`cat`](commands/cat.md)           | навигация / чтение        | Сервис     | без аппаратных ограничений | `UTILITIES`    | VFS; SD не обязательна                          |
| [`hexdump`](commands/hexdump.md)   | навигация / чтение        | Сервис     | без аппаратных ограничений | `UTILITIES`    | VFS; SD не обязательна                          |
| [`touch`](commands/touch.md)       | изменение / синхронизация | Сервис     | без аппаратных ограничений | `UTILITIES`    | VFS с нужной операцией записи                   |
| [`write`](commands/write.md)       | изменение / синхронизация | Сервис     | без аппаратных ограничений | `UTILITIES`    | VFS с нужной операцией записи                   |
| [`append`](commands/append.md)     | изменение / синхронизация | Сервис     | без аппаратных ограничений | `UTILITIES`    | VFS с нужной операцией записи                   |
| [`cp`](commands/cp.md)             | изменение / синхронизация | Сервис     | без аппаратных ограничений | `UTILITIES`    | VFS с нужной операцией записи                   |
| [`mv`](commands/mv.md)             | изменение / синхронизация | Сервис     | без аппаратных ограничений | `UTILITIES`    | VFS с нужной операцией записи                   |
| [`rename`](commands/rename.md)     | изменение / синхронизация | Сервис     | без аппаратных ограничений | `UTILITIES`    | VFS с нужной операцией записи                   |
| [`rm`](commands/rm.md)             | изменение / синхронизация | Сервис     | без аппаратных ограничений | `UTILITIES`    | VFS с нужной операцией записи                   |
| [`mkdir`](commands/mkdir.md)       | изменение / синхронизация | Сервис     | без аппаратных ограничений | `UTILITIES`    | VFS с нужной операцией записи                   |
| [`rmdir`](commands/rmdir.md)       | изменение / синхронизация | Сервис     | без аппаратных ограничений | `UTILITIES`    | VFS с нужной операцией записи                   |
| [`sync`](commands/sync.md)         | изменение / синхронизация | Сервис     | без аппаратных ограничений | `UTILITIES`    | VFS с нужной операцией записи                   |
| [`fs-info`](commands/fs-info.md)   | состояние файловых систем | Сервис     | без аппаратных ограничений | `DIAGNOSTICS`  | VFS / tmpfs                                     |
| [`tmp-info`](commands/tmp-info.md) | состояние файловых систем | Сервис     | без аппаратных ограничений | `DIAGNOSTICS`  | VFS / tmpfs                                     |
| [`mount`](commands/mount.md)       | управление mount          | Сервис     | FATFS                      | `ADMIN`        | FATFS + block device                            |
| [`umount`](commands/umount.md)     | управление mount          | Сервис     | FATFS                      | `ADMIN`        | FATFS + block device                            |
| [`yrx`](commands/yrx.md)           | передача файлов           | Сервис     | без аппаратных ограничений | `UTILITIES`    | терминальный канал + VFS                        |
| [`ysend`](commands/ysend.md)       | передача файлов           | Сервис     | без аппаратных ограничений | `UTILITIES`    | терминальный канал + VFS                        |
| [`edit`](commands/edit.md)         | редактирование текста     | Приложение | без аппаратных ограничений | `APPLICATIONS` | ANSI терминал + VFS; физический экран не нужен  |
| [`fs-smoke`](commands/fs-smoke.md) | проверка VFS              | Сервис     | без аппаратных ограничений | `TESTS`        | VFS с записью; отвязать тест от обязательной SD |

## Накопители

| Команда / документация                       | Операция                        | Уровень | Зависимости каталога                     | Группа        | Условия работы / примечания                        |
|----------------------------------------------|---------------------------------|---------|------------------------------------------|---------------|----------------------------------------------------|
| [`storage`](commands/storage.md)             | состояние накопителей/устройств | Драйвер | SD_CARD                                  | `DIAGNOSTICS` | block-device registry; devs пересекается с devices |
| [`block-devs`](commands/block-devs.md)       | состояние накопителей/устройств | Драйвер | SD_CARD                                  | `DIAGNOSTICS` | block-device registry; devs пересекается с devices |
| [`devs`](commands/devs.md)                   | состояние накопителей/устройств | Драйвер | SD_CARD                                  | `DIAGNOSTICS` | block-device registry; devs пересекается с devices |
| [`sd-baud`](commands/sd-baud.md)             | настройка SD                    | Драйвер | SD_CARD ∧ SD_SPI                         | `ADMIN`       | SD + поддержка операции выбранным backend          |
| [`sd-profile`](commands/sd-profile.md)       | настройка SD                    | Драйвер | SD_CARD ∧ SD_SPI                         | `ADMIN`       | SD + поддержка операции выбранным backend          |
| [`fat-cache`](commands/fat-cache.md)         | статистика FAT cache            | Сервис  | SD_CARD ∧ FATFS                          | `DIAGNOSTICS` | FATFS cache                                        |
| [`sd-test`](commands/sd-test.md)             | тест / диагностика SD           | Драйвер | SD_CARD ∧ SD_CARD_TEST_COMMANDS          | `TESTS`       | SD; SPI-операции только для SPI backend            |
| [`sd-spi-mode`](commands/sd-spi-mode.md)     | тест / диагностика SD           | Драйвер | SD_CARD ∧ SD_CARD_TEST_COMMANDS ∧ SD_SPI | `TESTS`       | SD; SPI-операции только для SPI backend            |
| [`sd-read-crc`](commands/sd-read-crc.md)     | тест / диагностика SD           | Драйвер | SD_CARD ∧ SD_CARD_TEST_COMMANDS          | `TESTS`       | SD; SPI-операции только для SPI backend            |
| [`sd-multiblock`](commands/sd-multiblock.md) | тест / диагностика SD           | Драйвер | SD_CARD ∧ SD_CARD_TEST_COMMANDS          | `TESTS`       | SD; SPI-операции только для SPI backend            |
| [`sd-multiwrite`](commands/sd-multiwrite.md) | тест / диагностика SD           | Драйвер | SD_CARD ∧ SD_CARD_TEST_COMMANDS          | `TESTS`       | SD; SPI-операции только для SPI backend            |
| [`sd-bench`](commands/sd-bench.md)           | тест / диагностика SD           | Драйвер | SD_CARD ∧ SD_CARD_TEST_COMMANDS          | `TESTS`       | SD; SPI-операции только для SPI backend            |
| [`sd-stress`](commands/sd-stress.md)         | тест / диагностика SD           | Драйвер | SD_CARD ∧ SD_CARD_TEST_COMMANDS          | `TESTS`       | SD; SPI-операции только для SPI backend            |
| [`sd-speed-scan`](commands/sd-speed-scan.md) | тест / диагностика SD           | Драйвер | SD_CARD ∧ SD_CARD_TEST_COMMANDS ∧ SD_SPI | `TESTS`       | SD; SPI-операции только для SPI backend            |
| [`sd-debug`](commands/sd-debug.md)           | тест / диагностика SD           | Драйвер | SD_CARD ∧ SD_CARD_TEST_COMMANDS          | `TESTS`       | SD; SPI-операции только для SPI backend            |

## Устройства, экран и ввод

| Команда / документация                           | Операция                  | Уровень    | Зависимости каталога            | Группа         | Условия работы / примечания                    |
|--------------------------------------------------|---------------------------|------------|---------------------------------|----------------|------------------------------------------------|
| [`devices`](commands/devices.md)                 | инвентаризация            | Сервис     | без аппаратных ограничений      | `DIAGNOSTICS`  | device registry                                |
| [`display-info`](commands/display-info.md)       | состояние экрана          | Драйвер    | DISPLAY                         | `DIAGNOSTICS`  | DisplayDevice                                  |
| [`display-power`](commands/display-power.md)     | питание экрана            | Драйвер    | DISPLAY                         | `ADMIN`        | DisplayDevice с power API                      |
| [`display-bars`](commands/display-bars.md)       | проверка отрисовки        | Драйвер    | DISPLAY ∧ DISPLAY_TEST_COMMANDS | `TESTS`        | DisplayDevice                                  |
| [`display-fill`](commands/display-fill.md)       | проверка отрисовки        | Драйвер    | DISPLAY ∧ DISPLAY_TEST_COMMANDS | `TESTS`        | DisplayDevice                                  |
| [`display-rect`](commands/display-rect.md)       | проверка отрисовки        | Драйвер    | DISPLAY ∧ DISPLAY_TEST_COMMANDS | `TESTS`        | DisplayDevice                                  |
| [`display-test`](commands/display-test.md)       | проверка отрисовки        | Драйвер    | DISPLAY ∧ DISPLAY_TEST_COMMANDS | `TESTS`        | DisplayDevice                                  |
| [`display-text`](commands/display-text.md)       | проверка отрисовки        | Драйвер    | DISPLAY ∧ DISPLAY_TEST_COMMANDS | `TESTS`        | DisplayDevice                                  |
| [`console-info`](commands/console-info.md)       | состояние LCD console     | Сервис     | LCD_CONSOLE                     | `DIAGNOSTICS`  | LCD console mirror                             |
| [`backlight`](commands/backlight.md)             | подсветка                 | Драйвер    | BACKLIGHT                       | `ADMIN`        | backlight service (экран или клавиатура)       |
| [`led`](commands/led.md)                         | индикация                 | Драйвер    | LED                             | `UTILITIES`    | status LED service                             |
| [`input-test`](commands/input-test.md)           | проверка ввода            | Драйвер    | KEYBOARD_TEST_COMMANDS          | `TESTS`        | соответствующий input/keyboard/gamepad device  |
| [`keyboard-live`](commands/keyboard-live.md)     | проверка ввода            | Драйвер    | KEYBOARD_TEST_COMMANDS          | `TESTS`        | соответствующий input/keyboard/gamepad device  |
| [`keyboard-raw`](commands/keyboard-raw.md)       | проверка ввода            | Драйвер    | KEYBOARD_TEST_COMMANDS          | `TESTS`        | соответствующий input/keyboard/gamepad device  |
| [`gamepad-test`](commands/gamepad-test.md)       | проверка ввода            | Драйвер    | KEYBOARD_TEST_COMMANDS          | `TESTS`        | соответствующий input/keyboard/gamepad device  |
| [`gamepad-live`](commands/gamepad-live.md)       | проверка ввода            | Драйвер    | KEYBOARD_TEST_COMMANDS          | `TESTS`        | соответствующий input/keyboard/gamepad device  |
| [`touch-info`](commands/touch-info.md)           | состояние touch           | Драйвер    | JC ∧ DISPLAY                    | `DIAGNOSTICS`  | TouchDevice                                    |
| [`touch-calibrate`](commands/touch-calibrate.md) | калибровка                | Драйвер    | JC ∧ DISPLAY                    | `ADMIN`        | TouchDevice; требуемые возможности калибровки  |
| [`touch-draw`](commands/touch-draw.md)           | проверка touch на экране  | Драйвер    | JC ∧ DISPLAY                    | `TESTS`        | TouchDevice ∧ DisplayDevice                    |
| [`mouse-info`](commands/mouse-info.md)           | состояние мыши/USB HID    | Драйвер    | JC                              | `DIAGNOSTICS`  | USB HID host; mouse-info — pointing device     |
| [`usb-hid-info`](commands/usb-hid-info.md)       | состояние мыши/USB HID    | Драйвер    | JC                              | `DIAGNOSTICS`  | USB HID host; mouse-info — pointing device     |
| [`mouse-reset`](commands/mouse-reset.md)         | сброс координат           | Драйвер    | JC                              | `ADMIN`        | pointing device                                |
| [`mouse-live`](commands/mouse-live.md)           | проверка событий мыши     | Драйвер    | JC                              | `TESTS`        | pointing device                                |
| [`imu`](commands/imu.md)                         | состояние / показания IMU | Драйвер    | CARDPUTER_ADV                   | `DIAGNOSTICS`  | IMU backend конкретной модели                  |
| [`ir`](commands/ir.md)                           | ИК управление             | Драйвер    | CARDPUTER_ADV                   | `UTILITIES`    | IR backend конкретной модели                   |
| [`gpio-analyzer`](commands/gpio-analyzer.md)     | захват GPIO               | Приложение | GPIO_ANALYZER ∧ FULL            | `EXPERIMENTAL` | GPIO capture ∧ display ∧ input ∧ memory budget |

## Аудио

| Команда / документация       | Операция                         | Уровень    | Зависимости каталога                     | Группа         | Условия работы / примечания              |
|------------------------------|----------------------------------|------------|------------------------------------------|----------------|------------------------------------------|
| [`audio`](commands/audio.md) | информация / тестовый тон        | Драйвер    | AUDIO                                    | `DIAGNOSTICS`  | AudioDevice; тон требует также TESTS     |
| [`mic`](commands/mic.md)     | захват / диагностика микрофона   | Драйвер    | MIC                                      | `TESTS`        | audio input backend                      |
| [`beep`](commands/beep.md)   | диагностический тон              | Драйвер    | PICOCALC ∧ AUDIO / PORTABLE_BEEP ∧ AUDIO | `TESTS`        | AudioDevice или PicoCalc PWM backend     |
| [`mplay`](commands/mplay.md) | MIDI playback (экспериментально) | Приложение | MIDI ∧ AUDIO                             | `APPLICATIONS` | MIDI ∧ AudioDevice ∧ VFS ∧ memory budget |

## Шины и PicoCalc

| Команда / документация                       | Операция                   | Уровень | Зависимости каталога | Группа        | Условия работы / примечания                       |
|----------------------------------------------|----------------------------|---------|----------------------|---------------|---------------------------------------------------|
| [`i2c-buses`](commands/i2c-buses.md)         | перечень шин               | Драйвер | I2C                  | `DIAGNOSTICS` | I²C registry                                      |
| [`i2c-scan`](commands/i2c-scan.md)           | активное сканирование шины | Драйвер | I2C                  | `TESTS`       | I²C backend                                       |
| [`picocalc-info`](commands/picocalc-info.md) | информация southbridge     | Драйвер | PICOCALC             | `DIAGNOSTICS` | southbridge service; direct-I²C режим — TESTS     |
| [`picocalc-sb`](commands/picocalc-sb.md)     | управление southbridge     | Драйвер | PICOCALC             | `ADMIN`       | southbridge service; низкоуровневые пробы — TESTS |
| [`psram-info`](commands/psram-info.md)       | состояние PSRAM            | Драйвер | PICOCALC ∧ PSRAM     | `DIAGNOSTICS` | PSRAM backend                                     |
| [`psram-test`](commands/psram-test.md)       | тест PSRAM                 | Драйвер | PICOCALC ∧ PSRAM     | `TESTS`       | PSRAM backend                                     |

## RTC и сеть

| Команда / документация                       | Операция                      | Уровень | Зависимости каталога       | Группа        | Условия работы / примечания                      |
|----------------------------------------------|-------------------------------|---------|----------------------------|---------------|--------------------------------------------------|
| [`rtc-get`](commands/rtc-get.md)             | часы / настройка              | Сервис  | без аппаратных ограничений | `ADMIN`       | RTC service, в том числе software RTC            |
| [`rtc-set`](commands/rtc-set.md)             | часы / настройка              | Сервис  | без аппаратных ограничений | `ADMIN`       | RTC service, в том числе software RTC            |
| [`rtc-config`](commands/rtc-config.md)       | часы / настройка              | Сервис  | без аппаратных ограничений | `ADMIN`       | RTC service, в том числе software RTC            |
| [`rtc-status`](commands/rtc-status.md)       | состояние часов               | Сервис  | без аппаратных ограничений | `DIAGNOSTICS` | RTC service                                      |
| [`rtc-sync`](commands/rtc-sync.md)           | сетевая синхронизация         | Сервис  | NET                        | `ADMIN`       | RTC ∧ network time backend                       |
| [`net-status`](commands/net-status.md)       | состояние сети                | Сервис  | NET                        | `DIAGNOSTICS` | network / Wi-Fi backend соответственно           |
| [`wifi-status`](commands/wifi-status.md)     | состояние сети                | Сервис  | WIFI                       | `DIAGNOSTICS` | network / Wi-Fi backend соответственно           |
| [`wifi-connect`](commands/wifi-connect.md)   | подключение / настройка       | Сервис  | WIFI                       | `ADMIN`       | Wi-Fi backend                                    |
| [`wifi-config`](commands/wifi-config.md)     | подключение / настройка       | Сервис  | WIFI                       | `ADMIN`       | Wi-Fi backend                                    |
| [`wifi-scan`](commands/wifi-scan.md)         | сканирование / проверка связи | Сервис  | WIFI                       | `UTILITIES`   | Wi-Fi scan / IP+ICMP соответственно              |
| [`ping`](commands/ping.md)                   | сканирование / проверка связи | Сервис  | NET                        | `UTILITIES`   | Wi-Fi scan / IP+ICMP соответственно              |
| [`eth`](commands/eth.md)                     | управление Ethernet           | Драйвер | JC                         | `ADMIN`       | Ethernet backend                                 |
| [`hosted-info`](commands/hosted-info.md)     | состояние coprocessor         | Драйвер | ESPIDF ∧ HOSTED            | `DIAGNOSTICS` | Hosted transport                                 |
| [`hosted-ota`](commands/hosted-ota.md)       | обновление coprocessor        | Драйвер | ESPIDF ∧ HOSTED            | `ADMIN`       | Hosted OTA; VFS/канал передачи согласно операции |
| [`hosted-ota-rx`](commands/hosted-ota-rx.md) | обновление coprocessor        | Драйвер | ESPIDF ∧ HOSTED            | `ADMIN`       | Hosted OTA; VFS/канал передачи согласно операции |

## Гостевые программы и Game Boy

| Команда / документация                       | Операция                | Уровень    | Зависимости каталога       | Группа         | Условия работы / примечания                                   |
|----------------------------------------------|-------------------------|------------|----------------------------|----------------|---------------------------------------------------------------|
| [`lexx-info`](commands/lexx-info.md)         | информация о .lexx      | Сервис     | без аппаратных ограничений | `UTILITIES`    | external-program loader ∧ VFS                                 |
| [`lexx-run`](commands/lexx-run.md)           | запуск .lexx            | Сервис     | без аппаратных ограничений | `APPLICATIONS` | RV32 VM ∧ VFS ∧ memory budget                                 |
| [`gbemu`](commands/gbemu.md)                 | эмуляция / настройка    | Приложение | GAMEBOY ∧ GB_RUNTIME       | `APPLICATIONS` | Game Boy ∧ VFS ∧ memory budget; display/input/audio по режиму |
| [`gbemu-config`](commands/gbemu-config.md)   | эмуляция / настройка    | Приложение | GAMEBOY                    | `APPLICATIONS` | Game Boy ∧ VFS ∧ memory budget; display/input/audio по режиму |
| [`gb-info`](commands/gb-info.md)             | анализ ROM              | Приложение | GAMEBOY                    | `DIAGNOSTICS`  | Game Boy core ∧ VFS                                           |
| [`gb-cart`](commands/gb-cart.md)             | анализ ROM              | Приложение | GAMEBOY                    | `DIAGNOSTICS`  | Game Boy core ∧ VFS                                           |
| [`gb-rom-check`](commands/gb-rom-check.md)   | анализ ROM              | Приложение | GAMEBOY                    | `DIAGNOSTICS`  | Game Boy core ∧ VFS                                           |
| [`gb-bus`](commands/gb-bus.md)               | тест / отладка Game Boy | Приложение | GAMEBOY                    | `TESTS`        | Game Boy core ∧ VFS; display только для экранных режимов      |
| [`gb-io`](commands/gb-io.md)                 | тест / отладка Game Boy | Приложение | GAMEBOY                    | `TESTS`        | Game Boy core ∧ VFS; display только для экранных режимов      |
| [`gb-irq`](commands/gb-irq.md)               | тест / отладка Game Boy | Приложение | GAMEBOY                    | `TESTS`        | Game Boy core ∧ VFS; display только для экранных режимов      |
| [`gb-ppu-test`](commands/gb-ppu-test.md)     | тест / отладка Game Boy | Приложение | GAMEBOY                    | `TESTS`        | Game Boy core ∧ VFS; display только для экранных режимов      |
| [`gb-vram-view`](commands/gb-vram-view.md)   | тест / отладка Game Boy | Приложение | GAMEBOY                    | `TESTS`        | Game Boy core ∧ VFS; display только для экранных режимов      |
| [`gb-cpu-step`](commands/gb-cpu-step.md)     | тест / отладка Game Boy | Приложение | GAMEBOY                    | `TESTS`        | Game Boy core ∧ VFS; display только для экранных режимов      |
| [`gb-test-run`](commands/gb-test-run.md)     | тест / отладка Game Boy | Приложение | GAMEBOY                    | `TESTS`        | Game Boy core ∧ VFS; display только для экранных режимов      |
| [`gb-test-suite`](commands/gb-test-suite.md) | тест / отладка Game Boy | Приложение | GAMEBOY                    | `TESTS`        | Game Boy core ∧ VFS; display только для экранных режимов      |

## Особые случаи и дальнейшая работа

- `audio` без TESTS сохраняет информацию об устройстве; тестовые тоны не компилируются.
- `picocalc-info` и `picocalc-sb` без TESTS сохраняют service-backed доступ; прямые I²C-пробы не компилируются.
- Отдельные отладочные команды Game Boy исключаются без TESTS. Внутренние trace/profile-параметры
  самого `gbemu` пока остаются частью приложения; их выделение — отдельная задача roadmap.
- `fs-smoke` относится к TESTS и VFS, больше не требует SD. `sd-baud`, `sd-profile`, `sd-spi-mode`,
  `sd-speed-scan` ограничены SPI backend. Остальные SD-тесты сохраняют runtime-проверки поддержанных операций.
- Гостевые [fileman, pacman, snake, tetris](../apps/README.md) собираются отдельно через LEXXOS_APPS/preset apps;
  их сборка не означает включение в firmware.
- [mplayer](commands/mplayer.md) — историческое имя; зарегистрирована `mplay`.

## Поддержание каталога

При добавлении команды задавайте name, group, requires, type, header, sources и doc в
[manifest](../cmake/commands.json). Источники регистрации больше не дублируются в групповых C++ файлах.
Перечень аппаратных возможностей нужно обновлять вместе с board providers. Изменения manifest/генератора
автоматически запускают CMake reconfigure. Проверки каталога находятся в
[tests/python/test_command_catalog.py](../tests/python/test_command_catalog.py).

[Генерация справочника конкретной сборки](command-docs.md) и `help` позволяют сверить фактический набор.
Матрица — читаемое описание; фактический состав задаёт manifest и effective конфигурация сборки.
