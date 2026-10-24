# Локальный OpenOCD для LexxOS

Цель настройки — использовать контролируемую сборку OpenOCD для загрузки и отладки Pico/RP2040 из CLion или терминала.
Документ описывает проектный wrapper `tools/openocd-local` и CMake targets для сборки OpenOCD из
`third_party/openocd`.

## Что делает проектная сборка

В проект добавлен CMake target:

```bash
lexxos_openocd
```

Он запускает `tools/scripts/lexxos-build-openocd.sh` и выполняет следующие шаги:

1. Проверяет, что `third_party/openocd` инициализирован.
2. Обновляет submodules OpenOCD:

```bash
git -C third_party/openocd submodule update --init --recursive
```

3. Временно добавляет в `third_party/openocd/src/flash/nor/spi.c` локальную запись SPI flash:

```cpp
FLASH_ID("unknown 25q16 0x681015", 0x03, 0x00, 0x02, 0xd8, 0xc7, 0x00151068, 0x100, 0x10000, 0x200000),
```

4. Запускает `./bootstrap` в исходниках OpenOCD.
5. Собирает OpenOCD в отдельном build-каталоге:

```text
build/pico/tools/openocd-build
```

6. Устанавливает результат в:

```text
build/pico/tools/openocd/bin/openocd
```

7. После завершения сборки восстанавливает исходный `spi.c`, если строка была добавлена скриптом.

Итог: OpenOCD собирается с локальным патчем, но submodule `third_party/openocd` после сборки не должен оставаться dirty.

## Первичная подготовка

После клонирования проекта и перед первой сборкой выполните:

```bash
git submodule update --init --recursive third_party/openocd third_party/FreeRTOS
```

На macOS для сборки OpenOCD обычно нужны autotools и библиотеки USB/HID:

```bash
brew install autoconf automake libtool pkg-config texinfo libusb hidapi libftdi
```

## Сборка из командной строки

Обычная конфигурация Pico:

```bash
cmake --preset debug-pico
```

Собрать локальный OpenOCD:

```bash
cmake --build --preset openocd-local
```

Очистить старую неудачную сборку OpenOCD и собрать заново:

```bash
cmake --build --preset openocd-local-clean
```

Собрать прошивку отдельно:

```bash
cmake --build --preset lexxos-pico
```

Проверить проектный wrapper:

```bash
./tools/openocd-local --version
```

## Почему используется shell workflow

OpenOCD — autotools-проект. На macOS надёжнее собирать его обычной последовательностью:

```bash
./bootstrap
configure
make
make install
```

Такой подход ближе к штатной сборке OpenOCD и не требует вручную вмешиваться в `CPPFLAGS`/`LDFLAGS`. Это важно, потому
что неверные флаги могут привести к смешиванию `x86_64` и `arm64` Homebrew libraries.

На Apple Silicon скрипт дополнительно использует:

```bash
-arch arm64
```

и передаёт `configure` triplet:

```bash
--build=aarch64-apple-darwin --host=aarch64-apple-darwin
```

Это защищает от смешивания Rosetta/x86_64 и arm64 Homebrew.

## Настройка CLion

1. Откройте `Settings | Build, Execution, Deployment | Embedded Development`.
2. В поле `OpenOCD location` укажите проектный wrapper:

```text
$PROJECT_DIR$/tools/openocd-local
```

Если CLion не принимает макрос в этом поле, выберите тот же файл через файловый диалог. Это будет абсолютный путь в
локальных настройках IDE, но сам wrapper останется переносимым внутри проекта.

3. Откройте `Run | Edit Configurations`.
4. Создайте конфигурацию `OpenOCD Download & Run`.
5. Заполните поля:

| Поле              | Значение                                    |
|-------------------|---------------------------------------------|
| Target            | `lexxos`                                    |
| Executable binary | `build/pico/src/lexxos.elf`                 |
| Board config file | `debug/openocd/lexxos-rp2040-cmsis-dap.cfg` |
| GDB port          | `3333`                                      |
| Telnet port       | `4444`                                      |
| Download          | `Always` или `If updated`                   |
| Reset             | `Halt` или `Run`, в зависимости от сценария |

Для Pico 2 / RP2350 позже можно использовать:

```text
debug/openocd/lexxos-rp2350-cmsis-dap.cfg
```

## Если CLion показывает `Wrong OpenOCD installation`

Проверьте wrapper из терминала:

```bash
./tools/openocd-local --version
```

Если wrapper сообщает, что бинарный файл не найден, соберите target:

```bash
cmake --build --preset openocd-local-clean
```

Если ошибка связана с правами на выполнение:

```bash
chmod +x tools/openocd-local tools/scripts/lexxos-build-openocd.sh
```

CMake module также пытается восстановить executable bits во время configure на Unix-like systems.

## Диагностика архитектуры macOS

Если линковщик сообщает о смешивании архитектур, проверьте:

```bash
uname -m
arch
brew --prefix
brew --prefix libusb
pkg-config --libs libusb-1.0
pkg-config --cflags libusb-1.0
file "$(brew --prefix libusb)/lib/libusb-1.0.dylib"
```

Для Apple Silicon нормальная комбинация обычно такая:

```text
uname -m -> arm64
brew --prefix -> /opt/homebrew
libusb-1.0.dylib -> arm64
```

## Важное ограничение

Путь к исполняемому файлу OpenOCD в CLion является настройкой IDE в `Embedded Development`, а не обычным CMake
cache-параметром проекта. Поэтому проект содержит переносимый wrapper `tools/openocd-local` и переносимые OpenOCD
cfg-файлы, но сам выбор OpenOCD в настройках CLion может потребовать одного локального действия на новой машине.
