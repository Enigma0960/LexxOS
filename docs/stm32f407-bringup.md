# Первичный запуск STM32F407

Информация о плате:
[STM32F407ZGT6 Euse M4 DEMO Large](https://stm32-base.org/boards/STM32F407ZGT6-Euse-M4-DEMO-Large.html).

Цель этапа: проверить плату, SWD, HSE/PLL, GPIO и USART1 до включения полного runtime LexxOS.

## Аппаратный профиль

- MCU: STM32F407ZGT6;
- отладка: SWD на PA13/PA14;
- консоль: USART1 на PA9/PA10, 115200 8N1;
- status LED: PC0;
- дисплей: EU-TFT3.2-320x240, отдельно описан в профиле платы.

## Toolchain

Preset использует Arm GNU Toolchain из Raspberry Pi Pico SDK:

```bash
${HOME}/.pico-sdk/toolchain/14_2_Rel1
```

Проверка:

```bash
${HOME}/.pico-sdk/toolchain/14_2_Rel1/bin/arm-none-eabi-gcc --version
${HOME}/.pico-sdk/toolchain/14_2_Rel1/bin/arm-none-eabi-gdb --version
```

## Конфигурация и сборка

```bash
cmake --preset debug-stm32f407
cmake --build --preset lexxos-stm32f407
```

Артефакты:

```text
build/stm32f407/src/lexxos.elf
build/stm32f407/src/lexxos.bin
build/stm32f407/src/lexxos.hex
build/stm32f407/src/lexxos.map
```

## Проверка OpenOCD

Wrapper проекта находит установленный OpenOCD в
`${HOME}/.pico-sdk/openocd-dev/_install/bin/openocd` или локальную сборку в
`${HOME}/.pico-sdk/openocd-dev/build/src/openocd`.

Проверка SWD:

```bash
./tools/openocd-lexxos -f debug/openocd/lexxos-stm32f407-cmsis-dap.cfg
```

Если порт 4444 занят:

```bash
lsof -nP -iTCP:4444 -sTCP:LISTEN
pkill -f openocd
```

Прошивка без CLion:

```bash
./tools/openocd-lexxos \
  -f debug/openocd/lexxos-stm32f407-cmsis-dap.cfg \
  -c "program build/stm32f407/src/lexxos.elf verify reset exit"
```

## Консоль UART

```text
Debug Probe UART RX -> PA9  / USART1_TX
Debug Probe UART TX -> PA10 / USART1_RX
Debug Probe GND     -> GND
```

Открытие терминала:

```bash
ls /dev/cu.usbmodem* /dev/cu.usbserial* 2>/dev/null
minicom -b 115200 -o -D /dev/cu.usbmodemXXXX
```

Ожидаемый вывод:

```text
LexxOS STM32F407 bring-up
board: Euse M4 DEMO Large
cpu: STM32F407ZGT6 Cortex-M4F
console: USART1 PA9/PA10 115200 8N1
flash base: 0x08000000
sram base:  0x20000000
type characters to test UART echo
lexx-stm32>
```

## CLion

Используйте конфигурацию запуска `LexxOS STM32F407`, ожидающую
`build/stm32f407/src/lexxos.elf`. Рекомендуемое имя toolchain: `ARM-PicoSDK`.

```text
Debugger: ${HOME}/.pico-sdk/toolchain/14_2_Rel1/bin/arm-none-eabi-gdb
OpenOCD:  <project>/tools/openocd-lexxos
```

## Память

Linker script размещает `.data`, `.bss` и stack в непрерывной SRAM объёмом 128 КиБ по адресу `0x20000000`. CCM SRAM
объёмом 64 КиБ по адресу `0x10000000` зарезервирована для явного размещения, поскольку подходит не для всех DMA и
периферийных сценариев.

Дисплей следует поднимать только после подтверждения SWD, UART и базового GPIO.

## Ошибка try_compile

Если CMake сначала находит `arm-none-eabi-gcc`, а затем не видит его внутри `try_compile`, пересоздайте каталог:

```bash
rm -rf build/stm32f407
cmake --preset debug-stm32f407
```

Toolchain-файл использует `LEXXOS_ARM_TOOLCHAIN_PATH` и стандартный путь Pico SDK. Для ручной проверки:

```bash
export PATH="$HOME/.pico-sdk/toolchain/14_2_Rel1/bin:$PATH"
```

