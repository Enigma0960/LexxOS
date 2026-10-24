# STM32F407: macOS, CLion, Raspberry Pi Debug Probe

Документ описывает подготовку macOS и CLion для target `stm32f407` платы
`STM32F407ZGT6 Euse M4 DEMO Large`.

Текущий проект уже содержит рабочую настройку OpenOCD для Pico. Для STM32F407 добавлены аналогичные файлы:

```text
debug/openocd/lexxos-stm32f407-cmsis-dap.cfg
.run/LexxOS STM32F407.run.xml
```

Важно: в проект добавлен минимальный STM32F407 bring-up target. Он уже собирает
`build/stm32f407/src/lexxos.elf`, который можно прошить и отлаживать через CLion/OpenOCD. Полный LexxOS runtime,
FreeRTOS, VFS, SDIO и дисплей будут переноситься следующими этапами.

## 1. Аппаратное подключение

Используется Raspberry Pi Debug Probe:

```text
Debug Probe SWD      STM32F407ZGT6 board
----------------------------------------
GND                  GND
SWDIO / SD           PA13 / SWDIO
SWCLK / SC           PA14 / SWCLK
VTref / 3V3          3.3V reference
RESET, optional      NRST
```

Для консоли LexxOS используется USART1:

```text
Debug Probe UART     STM32F407ZGT6 board
----------------------------------------
GND                  GND
RX                   PA9  / USART1_TX
TX                   PA10 / USART1_RX
```

RX/TX подключаются перекрёстно: вход probe `RX` к выходу микроконтроллера `TX`, выход probe `TX` к входу
микроконтроллера `RX`.

Для первого запуска рекомендуется питать плату обычным способом от USB платы, а Debug Probe использовать как SWD/UART и
подключать только общий GND и сигнальные линии. Не подавайте питание на плату одновременно из нескольких источников.

## 2. Инструменты macOS

Базовые инструменты:

```bash
xcode-select --install
brew update
brew install cmake ninja open-ocd arm-none-eabi-gcc minicom
```

Проверка:

```bash
cmake --version
ninja --version
openocd --version
arm-none-eabi-gcc --version
arm-none-eabi-gdb --version
```

Если Homebrew formula `arm-none-eabi-gcc` по какой-то причине не подходит, можно поставить официальный Arm GNU Toolchain
или Homebrew cask:

```bash
brew install --cask gcc-arm-embedded
```

После этого проверьте фактические пути:

```bash
which arm-none-eabi-gcc
which arm-none-eabi-gdb
which openocd
```

Типичные пути на Apple Silicon:

```text
/opt/homebrew/bin/openocd
/opt/homebrew/bin/arm-none-eabi-gcc
/opt/homebrew/bin/arm-none-eabi-gdb
```

Типичные пути на Intel Mac:

```text
/usr/local/bin/openocd
/usr/local/bin/arm-none-eabi-gcc
/usr/local/bin/arm-none-eabi-gdb
```

## 3. Проверка Debug Probe и SWD без CLion

Подключите SWD-линии и выполните из корня проекта:

```bash
openocd -f debug/openocd/lexxos-stm32f407-cmsis-dap.cfg
```

Нормальный результат: OpenOCD находит CMSIS-DAP adapter, видит Cortex-M target и открывает GDB server на `3333`.

Если соединение нестабильно, сначала уменьшите скорость в `debug/openocd/lexxos-stm32f407-cmsis-dap.cfg`:

```text
adapter speed 500
```

Если NRST подключён и target не удаётся остановить после неудачной прошивки, можно временно раскомментировать:

```text
reset_config srst_only srst_nogate
cortex_m reset_config sysresetreq
```

## 4. Проверка UART-консоли

Найдите порт Debug Probe:

```bash
ls /dev/cu.usbmodem* /dev/cu.usbserial* 2>/dev/null
```

Откройте терминал:

```bash
minicom -b 115200 -o -D /dev/cu.usbmodemXXXX
```

Для выхода из `minicom`: `Ctrl-A`, затем `X`.

На стороне LexxOS для этой платы нужно будет поднять USART1:

```text
USART1_TX = PA9
USART1_RX = PA10
baud      = 115200
```

## 5. TFT-дисплей EU-TFT3.2-320x240

Подключённый к плате `EU-TFT3.2-320x240` зафиксирован в board profile как параллельный 320x240 RGB565 TFT на `DIS`
-разъёме.
Это не мешает SWD и UART: SWD использует PA13/PA14, UART-консоль использует PA9/PA10, а основные линии дисплея находятся
на портах D/E/G. Подробности: `docs/stm32f407-display.md`.

## 6. Настройка CLion

### Toolchain

Откройте:

```text
Settings | Build, Execution, Deployment | Toolchains
```

Создайте toolchain `ARM`:

```text
Name:       ARM
CMake:      Bundled или /opt/homebrew/bin/cmake
Build Tool: /opt/homebrew/bin/ninja
C Compiler: /opt/homebrew/bin/arm-none-eabi-gcc
C++ Compiler: /opt/homebrew/bin/arm-none-eabi-g++
Debugger:   /opt/homebrew/bin/arm-none-eabi-gdb
```

Для Intel Mac замените `/opt/homebrew/bin` на `/usr/local/bin`, если Homebrew установлен туда.

### OpenOCD

Откройте:

```text
Settings | Build, Execution, Deployment | Embedded Development
```

В поле `OpenOCD location` укажите:

```text
/opt/homebrew/bin/openocd
```

или для Intel Mac:

```text
/usr/local/bin/openocd
```

Также можно использовать уже существующий wrapper проекта `tools/openocd-lexxos`, но для STM32F407 проще начать с
обычного Homebrew OpenOCD, потому что ему не нужен локальный Pico-specific patch.

### CMake configure/build

Из корня проекта:

```bash
cmake --preset debug-stm32f407
cmake --build --preset lexxos-stm32f407
```

После сборки должны появиться:

```text
build/stm32f407/src/lexxos.elf
build/stm32f407/src/lexxos.bin
build/stm32f407/src/lexxos.hex
build/stm32f407/src/lexxos.map
```

### Run configuration

В проект добавлена конфигурация:

```text
Run | Edit Configurations... | LexxOS STM32F407
```

Проверьте поля:

```text
Type:              OpenOCD Download & Run
Target:            lexxos
Executable binary: $PROJECT_DIR$/build/stm32f407/src/lexxos.elf
Board config file: $PROJECT_DIR$/debug/openocd/lexxos-stm32f407-cmsis-dap.cfg
GDB port:          3333
Telnet port:       4444
Download:          Always
Reset:             Init
Debugger:          ARM / arm-none-eabi-gdb
```

CMake profile `debug-stm32f407` уже добавлен. Конфигурация должна собирать именно этот ELF:

```text
build/stm32f407/src/lexxos.elf
```

## 6. Команды для ручной прошивки после появления ELF

```bash
openocd \
  -f debug/openocd/lexxos-stm32f407-cmsis-dap.cfg \
  -c "program build/stm32f407/src/lexxos.elf verify reset exit"
```

Ручной GDB-сеанс:

```bash
openocd -f debug/openocd/lexxos-stm32f407-cmsis-dap.cfg
```

Во втором терминале:

```bash
arm-none-eabi-gdb build/stm32f407/src/lexxos.elf
```

В GDB:

```gdb
target remote localhost:3333
monitor reset halt
load
break main
continue
```

## 7. Что нужно добавить в проект следующим патчем

Чтобы конфигурация `LexxOS STM32F407` стала полностью исполняемой, нужен отдельный порт:

```text
LEXXOS_PLATFORM=stm32
LEXXOS_BOARD=euse_m4_demo_stm32f407
```

Минимальный состав порта:

```text
CMake toolchain file для arm-none-eabi
linker script STM32F407ZG: FLASH 1M, RAM 192K
startup vector table
SystemInit / clock tree на HSE 8 MHz -> 168 MHz
platform_stm32.cpp
board_euse_m4_demo_stm32f407.cpp
USART1 backend PA9/PA10 115200
GPIO backend для LED/KEY
FreeRTOS Cortex-M4F port
```

Только после этого можно переходить к SDIO/FAT, дисплею и остальной периферии.
