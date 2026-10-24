# Сборка LexxOS

Начиная с версии 0.0.6, основной точкой входа для сборки является корень
репозитория. Все платформы должны переключаться через CMake presets из
корневого `CMakePresets.json`.

Не нужно открывать или конфигурировать отдельный проект из `ports/espidf`.
Этот каталог содержит ESP-IDF component, `sdkconfig.defaults`, partition table и
другие данные порта, но не является основным CMake-проектом.

## Инструменты гостевых приложений

При `LEXXOS_APPS=ON` (по умолчанию) любая сборка дополнительно требует Python 3 и RISC-V GCC/objcopy
либо Clang/LLD/llvm-objcopy. Компилятор прошивки выбирается независимо.
На Ubuntu CI устанавливает `gcc-riscv64-unknown-elf` и `binutils-riscv64-unknown-elf`.
Для сборки только ОС задайте `-DLEXXOS_APPS=OFF`; подробности — в [apps/README.md](../apps/README.md).

## Публичная матрица minimal / full

Для каждой из 12 плат есть configure и build preset с одинаковым именем:
`minimal-<board>` и `full-<board>`. Все они объявлены в `CMakePresets.json`;
`CMakeUserPresets.json` нужен только для личных переопределений.

```bash
cmake --preset minimal-dev_pico
cmake --build --preset minimal-dev_pico

tools/lexxos-build all-minimal --keep-going
tools/lexxos-build all-full --keep-going
# То же самое для выбранной платы:
tools/lexxos-build full-picocalc_pico2w
```

`all` означает все minimal-профили, `all-native` — minimal без ESP-IDF.
`--flavor full` переводит выбранные цели/группы в full; `--keep-going` продолжает
матрицу после ошибки и возвращает ненулевой код с перечнем неудачных целей.
Сборки выполняются последовательно, в отдельных `build/<preset>`.
Старые `debug-*` и performance presets сохранены для совместимости.

| Состав                                           | minimal               | full                          |
|--------------------------------------------------|-----------------------|-------------------------------|
| Shell, VFS, SD/FatFs, штатный экран и ввод       | Включены              | Включены                      |
| Доступные RTC, PSRAM, сетевые и аудиодрайверы    | По возможностям платы | По возможностям платы         |
| Game Boy, MIDI, health, аппаратные test commands | Выключены             | Включены с ограничениями ниже |
| GPIO analyzer                                    | Выключен              | Включён, если доступен плате  |
| Legacy FAT и legacy I2C debug/status task        | Выключены             | Выключены                     |

Экспериментальность сама по себе не исключает компонент из full. На PicoCalc Pico W
сохраняется ограничение RP2040: Game Boy, MIDI и GPIO analyzer выключены из-за SRAM.
Оба профиля этой платы используют FAT cache 4 сектора и RTOS heap 88 KiB.
Терминал RP2040 сохраняет обе ориентации LCD, но имеет 32 строки истории и очередь
2048 символов; буфер передачи RGB888 ограничен 512 пикселями. Для `dev_pico` и
`dev_pico_sdio` full heap — 96 KiB. Линковка не подтверждает запас памяти во всех
сценариях: runtime heap/stack нужно проверить на оборудовании.

`dev_pico`, `dev_pico_sdio`, `dev_pico2w` экспериментальны: настройки и назначение IO
могут меняться. SDIO включён явно только в соответствующем профиле. У PicoCalc
публичные presets рассчитаны на JCS firmware клавиатуры; для factory firmware
задайте `-DLEXXOS_PICOCALC_KEYBOARD_FIRMWARE=factory` при конфигурации.

## Требования и сборка на новой машине

- Для публичных presets нужен CMake **3.25+**: файл использует schema version 6.
  Прямая конфигурация через `cmake -S . -B ...` имеет минимум 3.20 из CMakeLists.
  Соответствие версий описано
  в [CMake presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html#versions).
- Нужны Ninja и C/C++20 toolchain; для static checks — Python 3.
- Host/simulator не требуют Pico SDK, FreeRTOS или ESP-IDF.
- Pico presets настроены на Pico SDK 2.2.0 и Arm GNU 14_2_Rel1 в `~/.pico-sdk/`.
  Это значения по умолчанию, а не автоматическая установка SDK.
- Для Pico/STM32 нужен `git submodule update --init --recursive third_party/FreeRTOS`.
- Для ESP-IDF активируйте `export.sh` выбранного SDK. Зависимости фиксируются отдельно для каждого
  IDF target в `ports/espidf/locks/`; проверяемая версия SDK — 6.0.2.
  Не запускайте ESP-IDF сборки разных target одновременно: `managed_components` общий.

Если SDK расположен иначе, задайте пути явно (замените `/path/to/...` своими путями):

```bash
cmake --preset debug-pico \
  -DPICO_SDK_PATH=/path/to/pico-sdk \
  -DPICO_TOOLCHAIN_PATH=/path/to/arm-toolchain
cmake --build --preset lexxos-pico

cmake --preset debug-stm32f407 \
  -DLEXXOS_ARM_TOOLCHAIN_PATH=/path/to/arm-toolchain
cmake --build --preset lexxos-stm32f407
```

Для повторяемой проверки используйте отдельный build directory:

```bash
cmake --preset debug-pico -B build/check-pico \
  -DPICO_SDK_PATH=/path/to/pico-sdk \
  -DPICO_TOOLCHAIN_PATH=/path/to/arm-toolchain
cmake --build build/check-pico --target lexxos
```

После `-B` сборка запускается по этому каталогу: build preset продолжает указывать на штатный путь.
`tools/lexxos-build check all` проверяет наличие инструментов, но не компиляцию, линковку или hardware smoke.
Текущие результаты и ограничения — в [итоговом отчёте 0.1.0](tests/release-0.1.0-validation.md).

## Проверка доступных профилей

Из корня проекта:

```bash
cmake --list-presets
cmake --build --list-presets
```

Если терминал открыт не в корне проекта, можно использовать wrapper:

```bash
tools/lexxos-cmake --list-presets
tools/lexxos-cmake --build --list-presets
```

Wrapper всегда запускает CMake из корня репозитория, поэтому его удобно
использовать из `src`, `ports/espidf`, `docs` и других подкаталогов.

## Быстрая сборка через build dispatcher

Для повседневной работы можно использовать `tools/lexxos-build`. Он работает
поверх корневых CMake presets и запускает `cmake` из корня репозитория, как и
`tools/lexxos-cmake`.

Список доступных целей:

```bash
tools/lexxos-build list
```

Частые команды:

```bash
tools/lexxos-build check
tools/lexxos-build check all
tools/lexxos-build check esp32
```

Команда `check` не запускает сборку. Она проверяет корень репозитория, `cmake`,
`ninja`, FreeRTOS, Pico SDK/toolchain и наличие ESP-IDF `export.sh` для ESP32.

```bash
tools/lexxos-build host
tools/lexxos-build pico
tools/lexxos-build stm32
tools/lexxos-build esp32
```

Сборка основных не-ESP целей:

```bash
tools/lexxos-build all-native
```

Сборка всех основных целей, включая ESP32:

```bash
tools/lexxos-build all
```

Для ESP32 `tools/lexxos-build` автоматически вызывает `export.sh` перед
запуском CMake. Путь берётся из `--idf-path`, `LEXXOS_IDF_PATH`, `IDF_PATH` или
из типовых каталогов установки ESP-IDF. Если нужно использовать уже активное
окружение без автоматического вызова `export.sh`, можно передать
`--no-espidf-export`.

Для ESP32 также доступны `--flash` и `--app-flash`:

```bash
tools/lexxos-build esp32 --app-flash
```

Перед сборкой можно проверить окружение:

```bash
tools/lexxos-build check
tools/lexxos-build check all
tools/lexxos-build check esp32
```

## Feature Flags

`LEXXOS_FEATURE_DISPLAY` включает основные display drivers. Legacy I2C debug/status display управляется отдельным
`LEXXOS_FEATURE_DEBUG_DISPLAY`: по умолчанию он следует за display feature, но отключается для PicoCalc-профилей.
Debug display source files добавляются только при включённом `LEXXOS_FEATURE_DEBUG_DISPLAY`.

`LEXXOS_FEATURE_MIDI` включён по умолчанию в full и выключен в minimal/standard. Он включает экспериментальную команду
`mplayer`; для релизной standard
сборки 0.0.7 MIDI playback не входит в набор команд, пока аудио-путь не проверен на реальном PicoCalc.

Для `LEXXOS_BOARD=picocalc_pico_w` действует отдельный tight-RAM профиль под RP2040: CMake выставляет пустой
`LEXXOS_PICO_FREERTOS_HEAP_KB` в 88 KiB и ограничивает старые cache values сверху этим значением. Без явного override
профиль выключает `LEXXOS_FEATURE_GAMEBOY`,
`LEXXOS_FEATURE_MIDI` и `LEXXOS_FEATURE_GPIO_ANALYZER`. Это не runtime stack policy, а защита от link-time
переполнения SRAM, когда `.bss`/`.heap` начинают пересекать Pico SDK stack dummy sections. Override
`LEXXOS_ALLOW_PICOCALC_PICO_W_HEAVY_FEATURES=ON` нужен только для экспериментальной RP2040-сборки, не для релизного
профиля 0.0.7.

Для того же RP2040-профиля CMake ограничивает `LEXXOS_PICOCALC_PSRAM_SPI_BAUDRATE` сверху значением `8000000`. Hardware
retest поймал неверный PSRAM readback при 16 MHz из старого cache; релизный Pico W образ должен использовать
hardware-confirmed 8 MHz PSRAM PIO profile. Pico 2 W/RP2350 этим cap не ограничивается.

`display-test` больше не держит два 40 KiB benchmark buffer в static `.bss` и не резервирует benchmark workspace для
обычного pattern mode. `display-test --bench` лениво выделяет один или два RGB565 chunk buffer из heap под активный
transport/rectangle; дефолтный `--rows` равен 16. При слишком плотном heap benchmark mode должен отказать с
`display-test: not enough heap for benchmark workspace`, а не падать через FreeRTOS malloc hook.

`LEXXOS_FEATURE_KEYBOARD` управляет optional input adapters: matrix/PCF8575 keyboard, keyboard gamepad adapter,
Sega Genesis gamepad через PCF8575, simulator input и M5Stack Basic buttons. PicoCalc keyboard и Cardputer keyboard
собираются по board-family/profile guards (`FEATURE_PICO_CALC`/`FEATURE_CARDPUTER`) и не зависят напрямую от этого
флага; для PicoCalc через `LEXXOS_FEATURE_KEYBOARD` дополнительно включается только gamepad adapter поверх keyboard
events.

## Экспериментальный GUI для сборки

Для ручной работы с профилями можно использовать простой GUI поверх
`tools/lexxos-build`:

```bash
tools/lexxos-build-gui
```

GUI не содержит собственной логики сборки и запускает тот же dispatcher, что и
командная строка. Для работы нужен Python-пакет `PySide6`.

## GUI-конфигуратор

`tools/lexxos-build-gui` теперь является тонким launcher-скриптом. Основной код
GUI находится в каталоге `tools/build-tool`, чтобы его можно было отдельно
отлаживать, упаковывать и позже заменить standalone-бинарником в `tools/`.

Установка зависимостей для запуска из исходников:

```bash
python3 -m pip install -r tools/build-tool/requirements.txt
```

Запуск:

```bash
tools/lexxos-build-gui
```

GUI предназначен в первую очередь как конфигуратор: выбор платформы, платы,
готового preset или ручной CMake-конфигурации, путей к SDK/toolchain/debugger и
набора компонентов. Запуск и отладку прошивки по-прежнему удобнее выполнять из
CLion, открывая корень репозитория и выбирая соответствующий CMake profile.

Настройки GUI сохраняются через `QSettings`: для каждой платформы хранится свой
последний набор полей, поэтому переключение между Pico, STM32F407 и ESP-IDF не
сбрасывает введённые пути и компоненты.

В GUI есть вспомогательные кнопки:

- `Export CMakeUserPresets.json` — записать текущую конфигурацию как локальный
  CMake preset;
- `Open CMakeUserPresets.json` — открыть локальный файл пользовательских preset;
- `Open build dir` — открыть текущий build directory;
- `Reset platform settings` — сбросить сохранённые значения только для текущей
  платформы.

### Standalone build-tool

После отладки GUI можно собрать standalone-исполняемый файл без необходимости
устанавливать PySide6 на машине пользователя:

```bash
python3 -m pip install -r tools/build-tool/requirements.txt
python3 tools/build-tool/package.py
```

По умолчанию результат копируется в:

```text
tools/lexxos-build-tool
```

После этого конфигуратор можно запускать так:

```bash
tools/lexxos-build-tool
```

## Пользовательские presets и CLion

Для локальных конфигураций, созданных через `tools/lexxos-build-gui`, используется
корневой файл `CMakeUserPresets.json`. Он находится рядом с `CMakePresets.json`,
автоматически читается CMake и не должен коммититься.

Пример проверки из терминала:

```bash
cmake --preset user-pico-dev_pico
cmake --build --preset user-pico-dev_pico-build
```

В CLion отдельный путь к `CMakeUserPresets.json` обычно указывать не нужно:
открой корень репозитория, затем используй `Tools | CMake | Reload CMake Project`
или перезагрузи проект. Пользовательские presets из `CMakeUserPresets.json`
появятся в списке CMake profiles рядом с профилями из `CMakePresets.json`.

Если CLion уже был открыт до экспорта, после записи `CMakeUserPresets.json` нужно
перечитать CMake project. Если профиль не появился, проверь, что файл лежит именно
в корне репозитория и валиден:

```bash
cmake --list-presets
```

## Host

```bash
cmake --preset debug-host
cmake --build --preset debug-host
```

Через wrapper:

```bash
tools/lexxos-cmake --preset debug-host
tools/lexxos-cmake --build --preset debug-host
```

## Raspberry Pi Pico

```bash
cmake --preset debug-pico
cmake --build --preset lexxos-pico
```

Полный профиль:

```bash
cmake --preset debug-pico-full
cmake --build --preset lexxos-pico-full
```

## STM32F407

```bash
cmake --preset debug-stm32f407
cmake --build --preset lexxos-stm32f407
```

Полный профиль:

```bash
cmake --preset debug-stm32f407-full
cmake --build --preset lexxos-stm32f407-full
```

## ESP32 / ESP-IDF

Перед сборкой нужно активировать окружение ESP-IDF:

```bash
. "$IDF_PATH/export.sh"
```

Затем из корня проекта:

```bash
cmake --preset debug-esp32
cmake --build --preset lexxos-esp32
```

Прошивка:

```bash
cmake --build --preset lexxos-esp32-flash
```

Прошивка только приложения:

```bash
cmake --build --preset lexxos-esp32-app-flash
```

Через wrapper из любого каталога:

```bash
tools/lexxos-cmake --preset debug-esp32
tools/lexxos-cmake --build --preset lexxos-esp32
```

## CLion

В CLion нужно открывать корень репозитория LexxOS. Переключение платформы
выполняется выбором CMake profile:

- `debug-host`
- `debug-pico`
- `debug-pico-full`
- `debug-stm32f407`
- `debug-esp32`

Каталог `ports/espidf` не нужно открывать как отдельный CLion-проект.

## Гостевые приложения из /apps

```sh
cmake --preset apps
cmake --build --preset apps
```

Отдельный таргет `apps` собирает все гостевые программы без прошивки и SDK плат. Результаты: `build/apps/*.lexx`.
Требования к инструментам и добавление новых программ: [apps/README.md](../apps/README.md).

## Группы встроенных команд

Состав команд управляется `LEXXOS_COMMANDS_SYSTEM`, `UTILITIES`, `ADMIN`, `DIAGNOSTICS`,
`APPLICATIONS`, `TESTS`, `EXPERIMENTAL` (для каждого используется полный префикс `LEXXOS_COMMANDS_`).
Аппаратные feature-флаги продолжают управлять драйверами. Выключение группы убирает её команды,
но сохраняет общие сервисы ОС. `BUILD_TESTING` и группы firmware-команд независимы.
Примеры, зависимости и defaults: [матрица команд](command-matrix.md).

## Релизные архивы

Для Cardputer ADV, четырёх PicoCalc и JC ESP32-P4 доступны автоматические minimal/full
архивы и отдельный apps ZIP. Из чистого рабочего дерева:

```sh
cmake --preset release
cmake --build --preset release
```

Состав, требования и дополнительные параметры: [упаковка релиза](release-packaging.md).
