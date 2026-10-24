# Настройка CLion

Пути `/path/to/...` условные. В полях CLion укажите абсолютные пути к своей
копии проекта, ARM toolchain и OpenOCD; в командах терминала замените их перед запуском.

Документ описывает рекомендуемую настройку CLion для двух основных вариантов сборки LexxOS:

- `debug-host` — локальная host-сборка для запуска на macOS или другой desktop-системе;
- `debug-pico` — embedded-сборка прошивки для Raspberry Pi Pico / RP2040.

## Почему CLion показывает больше двух профилей

В проекте есть `CMakePresets.json` с configure presets:

```text
debug-host
debug-pico
```

и build presets:

```text
debug-host
debug-pico
lexxos-pico
openocd-local
openocd-local-clean
```

CLion может автоматически импортировать и configure presets, и build presets. Поэтому в окне
`Settings | Build, Execution, Deployment | CMake` могут появляться не только два базовых профиля, но и производные
варианты вида:

```text
debug-host preset
debug-pico preset
debug-host - debug-host preset
debug-pico - debug-pico preset
debug-pico - lexxos-pico preset
```

Если дополнительно были созданы ручные профили CLion, например `Debug Host` и `Debug Raspberry Pi Pico`, они будут
показаны рядом с импортированными preset-профилями. Это не ошибка сборки, а следствие автоматического импорта CMake
presets в CLion.

## Рекомендуемый вариант: оставить только configure presets

Чтобы в CLion остались только два рабочих профиля, а командные build presets продолжили работать из терминала:

1. Откройте `Settings | Advanced Settings | CMake`.
2. Найдите настройку `Automatically import CMake Presets`.
3. Установите значение `Configure`.
4. Выполните `Reset Cache and Reload Project` или переоткройте проект.
5. В `Settings | Build, Execution, Deployment | CMake` удалите или отключите ручные дублирующие профили.
6. Оставьте включёнными только:

```text
debug-host preset
debug-pico preset
```

После этого переключение между host и Pico выполняется через selector CMake profile в CLion.

## Какой профиль выбирать

Для локального запуска shell-версии используйте:

```text
debug-host preset
```

Для сборки прошивки RP2040 используйте:

```text
debug-pico preset
```

Признак правильной Pico-сборки — после сборки target `lexxos` появляются файлы:

```text
build/pico/src/lexxos.elf
build/pico/src/lexxos.bin
build/pico/src/lexxos.hex
build/pico/src/lexxos.uf2
```

Если в build-каталоге есть только файл `src/lexxos` без `lexxos.elf`, значит выбран host-профиль, а не Pico-профиль.

## Проверка из терминала

Host-сборка:

```bash
cmake --preset debug-host
cmake --build --preset debug-host
./build/host/src/lexxos
```

Pico-сборка:

```bash
cmake --preset debug-pico
cmake --build --preset lexxos-pico
find build/pico -maxdepth 3 -type f -name "lexxos*" -print
```

Ожидаемый ELF-файл для OpenOCD:

```text
build/pico/src/lexxos.elf
```

## Настройка toolchain для Pico

Откройте `Settings | Build, Execution, Deployment | Toolchains` и создайте toolchain, например `RPI`.

Рекомендуемые значения:

| Поле       | Значение                                                                |
|------------|-------------------------------------------------------------------------|
| CMake      | `Bundled`                                                               |
| Build Tool | `Ninja`                                                                 |
| Debugger   | `/path/to/pico-toolchain/bin/arm-none-eabi-gdb` |

Компиляторы C/C++ можно оставить автоматически определёнными. Для Pico SDK реальный ARM toolchain задаётся через
`PICO_TOOLCHAIN_PATH` в `debug-pico` preset.

## Настройка OpenOCD в CLion

Откройте `Settings | Build, Execution, Deployment | Embedded Development`.

В поле `OpenOCD location` можно указать один из двух вариантов.

Глобальная локальная сборка OpenOCD:

```text
/path/to/openocd/bin/openocd
```

Проектный wrapper, если используется сборка из `docs/openocd-local.md`:

```text
$PROJECT_DIR$/tools/openocd-local
```

Проверка из терминала для глобальной сборки:

```bash
/path/to/openocd/bin/openocd --version
```

Проверка проектного wrapper:

```bash
./tools/openocd-local --version
```

## Настройка OpenOCD Download & Run

Откройте `Run | Edit Configurations...` и создайте конфигурацию `OpenOCD Download & Run`.

Рекомендуемые значения для RP2040:

| Поле              | Значение                                                                |
|-------------------|-------------------------------------------------------------------------|
| Name              | `LexxOS`                                                                |
| Target            | `lexxos`                                                                |
| Executable binary | `$PROJECT_DIR$/build/pico/src/lexxos.elf`                               |
| Debugger          | `/path/to/pico-toolchain/bin/arm-none-eabi-gdb` |
| Board config file | `$PROJECT_DIR$/debug/openocd/lexxos-rp2040-cmsis-dap.cfg`               |
| GDB port          | `3333`                                                                  |
| Telnet port       | `4444`                                                                  |
| Download          | `Always` или `If updated`                                               |
| Reset             | `Run`                                                                   |

В `Before launch` должен быть шаг `Build`, чтобы перед прошивкой CLion собирал свежий target `lexxos`.

Для первой настройки можно использовать абсолютный путь к ELF-файлу:

```text
/path/to/LexxOS/build/pico/src/lexxos.elf
```

Это исключает ошибку с путём вида:

```text
build-debug-rpi/src/src/lexxos.elf
```

После того как конфигурация заработает, путь можно заменить на макрос CLion, если он раскрывается корректно в вашей
версии IDE.

## Порядок запуска отладки

1. Выберите CMake profile `debug-pico preset`.
2. Выполните `Reload CMake Project`.
3. Соберите target `lexxos`.
4. Убедитесь, что существует файл `build/pico/src/lexxos.elf`.
5. Поставьте breakpoint в коде.
6. Запустите конфигурацию `OpenOCD Download & Run` кнопкой `Debug`, а не `Run`.

При корректной настройке CLion прошьёт ELF через OpenOCD, подключит `arm-none-eabi-gdb`, запустит плату и остановится на
установленной точке останова.

## Типовые ошибки

### Есть только `lexxos`, но нет `lexxos.elf`

Выбран host-профиль. Нужно выбрать `debug-pico preset` и пересобрать проект.

### Путь содержит `src/src/lexxos.elf`

В `Executable binary` указан некорректный макрос или путь. Укажите абсолютный путь:

```text
/path/to/LexxOS/build/pico/src/lexxos.elf
```

### CLion запускает не тот OpenOCD

Проверьте `Settings | Build, Execution, Deployment | Embedded Development | OpenOCD location` и убедитесь, что там
указан
путь к нужной сборке OpenOCD.

В логе запуска OpenOCD должна быть строка с вашей dev-сборкой.

### Предупреждения `DEPRECATED! use 'gdb port'`

Это предупреждения от команд, которые генерирует CLion для OpenOCD. Они не мешают прошивке и отладке.

### CLion не видит проектный wrapper

Проверьте права на выполнение:

```bash
chmod +x tools/openocd-local tools/scripts/lexxos-build-openocd.sh
```

Также можно указать OpenOCD абсолютным путём в `Embedded Development`.
