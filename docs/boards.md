# Профили плат LexxOS

Документ описывает board profiles, которые есть или зарезервированы в проекте. Board profile задаёт pinout и
capabilities конкретной платы, но не является platform backend и не должен содержать логику драйверов.

## Экспериментальные dev-платы

`dev_pico`, `dev_pico_sdio` и `dev_pico2w` — экспериментальные платы для разработки и проверки драйверов. Их настройки, состав периферии и назначение IO (GPIO/pinout) могут меняться со временем. Перед подключением оборудования сверяйте wiring с документацией и board descriptor используемой версии; совместимость распиновки между версиями не гарантируется.

## `host_native`

Локальная host-сборка для запуска shell без микроконтроллера.

Назначение:

- проверка shell и program manager;
- проверка VFS/tmpfs без платы;
- быстрая отладка команд;
- проверка CMake-конфигурации.

Физических GPIO, UART, SPI, I2C и SD card-detect у этого профиля нет. Host backend предоставляет stubs там, где это
нужно для сборки переносимого кода.

## `dev_pico` — экспериментальная плата

Основной текущий development board profile для Raspberry Pi Pico / RP2040.

```text
Processor: RP2040
Board id:  dev_pico
```

Pinout:

```text
UART0 TX: GP0
UART0 RX: GP1
UART baud: 115200

PSRAM CS:   GP9
PSRAM SCK:  GP10
PSRAM MOSI: GP11
PSRAM MISO: GP12
PSRAM SIO2: GP13
PSRAM SIO3: GP14

SD MISO: GP16
SD CS:   GP17
SD SCK:  GP18
SD MOSI: GP19
SD CD:   GP22 active-low

OLED SDA: GP20
OLED SCL: GP21
OLED bus: I2C0
OLED controller: SSD1306-compatible 128x64 I2C
OLED address: 0x3C

STATUS LED: GP25 или board default
```

Card-detect использует GP22 как active-low input. Ожидаемая логика:

```text
карта не установлена: GPIO = 1
карта установлена:    GPIO = 0
```

Platform backend включает internal pull-up для active-low схемы. Для финальной платы желательно добавить внешний pull-up
примерно 10 кОм к 3.3 В.

## `dev_pico_sdio` — экспериментальная плата

Экспериментальный профиль для будущей проверки RP2040 SDIO/PIO backend. На текущем этапе это только guarded skeleton,
а не production-профиль. Обычная рабочая плата должна продолжать использовать `dev_pico` и SPI backend.

```text
Processor: RP2040
Board id:  dev_pico_sdio
SD interface: SDIO 4-bit, experimental
```

Предварительная распиновка:

```text
SDIO DAT0: GP16
SDIO DAT1: GP17
SDIO DAT2: GP18
SDIO DAT3: GP19
SDIO CMD:  GP20
SDIO CLK:  GP21
SDIO CD:   GP22 active-low
```

Этот профиль конфликтует с текущим `dev_pico` pinout:

```text
SPI SD:    GP16..GP19
OLED I2C:  GP20/GP21
SDIO:      GP16..GP21
```

Поэтому SDIO оформлен отдельным board profile. Чтобы случайно не заменить стабильный SPI-путь, сборка RP2040 SDIO
требует явного включения:

```text
LEXXOS_BOARD=dev_pico_sdio
LEXXOS_SD_BACKEND=sdio
LEXXOS_ENABLE_EXPERIMENTAL_RP2040_SDIO=ON
```

Подробный production-статус SD/FAT зафиксирован в `docs/sd-card-production.md`.

## `dev_pico2w` — экспериментальная плата

Профиль для Raspberry Pi Pico 2 W / RP2350.

Он сохраняет общую идею внешнего development pinout, включая UART, SD SPI и debug OLED, но card-detect пока считается
неподтверждённым для этого профиля. Перед включением hot-plug на Pico 2 W нужно отдельно проверить wiring и уровень
сигнала.

```text
Processor: RP2350
Board id:  dev_pico2w
```

## PicoCalc: выбор модуля для 0.1.0

| Модуль | Board id | Pico SDK board | Сеть |
| --- | --- | --- | --- |
| Pico | `picocalc_pico` | `pico` | Нет CYW43 |
| Pico W | `picocalc_pico_w` | `pico_w` | Wi-Fi |
| Pico 2 | `picocalc_pico2` | `pico2` | Нет CYW43 |
| Pico 2 W | `picocalc_pico2w` | `pico2_w` | Wi-Fi |

Для каждого есть `minimal-<board id>` и `full-<board id>`. В Pico/Pico 2 без W
используется общая PicoCalc-распиновка соответствующего поколения, wireless отключён.
Все четыре варианта собраны; аппаратная проверка финальных образов не выполнена.
Публичные presets используют `jcs` southbridge. Ограничения SRAM RP2040 относятся
к Pico и Pico W. [Установка и конфигурация](boards/picocalc_pico_w.md).

## `picocalc_pico_w`

Рабочий профиль для ClockworkPi PicoCalc с Raspberry Pi Pico W / RP2040. Профиль использует Pico platform backend, но
отдельно описывает интерфейсы платы и подключённые устройства.

```text
Processor: RP2040
Board id:  picocalc_pico_w
```

Релизный профиль 0.0.7 использует tight-RAM конфигурацию RP2040: FreeRTOS heap фиксируется на 88 KiB, а Game Boy,
MIDI playback и GPIO analyzer выключены без экспериментального override. Это отделяет поддержанный Pico W образ от
более широкого Pico 2 W/RP2350 regression-профиля и предотвращает link-time SRAM overflow.

Интерфейсы:

```text
Console UART: UART0 GP0/GP1, 115200
USB CDC:      console mirror enabled by default for PicoCalc, product string "LexxOS PicoCalc"
System I2C:   I2C1 GP6/GP7, 400 kHz
Storage SPI:  SPI0 GP18/GP19/GP16, stable production 50 MHz
Display SPI:  SPI1 GP10/GP11
Wireless:     CYW43 Wi-Fi/Bluetooth capability, Wi-Fi scan confirmed on PicoCalc Pico W
PSRAM:        CE GP20, SCLK GP21, SI GP2, SO GP3, SIO2 GP4, SIO3 GP5, JEDEC ID confirmed
Audio beep:   left GP26, right GP27, left/right smoke-test confirmed
```

Устройства:

```text
Keyboard: PicoCalc STM32 southbridge, I2C address 0x1F on system I2C
Display:  ILI9488-compatible 320x320 RGB565 LCD, CS GP13, DC GP14, RESET GP15
SD card:  SPI SD, CS GP17, CD GP22 active-low, FAT automount to /mnt/sd
```

LCD используется как основная экранная консоль. Display diagnostics временно переводят LCD mirror в graphics mode на
время отрисовки и возвращают text mode после завершения команды. `display-power [on|off|sleep]` управляет режимом LCD
controller; на PicoCalc эта команда также гасит/восстанавливает LCD и keyboard backlight через southbridge service,
клавиатура будит display из `off`/`sleep`, а `display-power timeout` хранит idle timeout в `/mnt/sd/.lexx`. Диагностика
доступна командой:

```text
console-info
psram-info
beep
wifi-scan
```

Подробности:

- `docs/boards/picocalc_pico_w.md` — сборка, прошивка и smoke-test;
- `docs/picocalc-southbridge.md` — STM32 southbridge, keyboard FIFO ownership и безопасная диагностика;
- `docs/picocalc-lcd-console.md` — экранная консоль, scrollback и сочетания клавиш.

## `picocalc_pico2w`

Профиль для ClockworkPi PicoCalc с Raspberry Pi Pico 2 W / RP2350.

```text
Processor: RP2350
Board id:  picocalc_pico2w
Platform:  pico
SDK board: pico2_w
Console:   UART0, TX GP0, RX GP1, 115200 baud
USB CDC:   console mirror enabled by default for PicoCalc, product string "LexxOS PicoCalc"
I2C:       I2C1, SDA GP6, SCL GP7, 400 kHz
SD:        SPI0, MISO GP16, CS GP17, SCK GP18, MOSI GP19, card detect GP22 active-low
LCD:       ILI9488-compatible 320x320 panel on SPI1, SCK GP10, MOSI GP11, CS GP13, DC GP14, RESET GP15
Wireless:  CYW43 Wi-Fi/Bluetooth capability, Wi-Fi scan/connect/status/ping smoke-test
PSRAM:     CE GP20, SCLK GP21, SI GP2, SO GP3, SIO2 GP4, SIO3 GP5
Audio:     beep PWM left GP26, right GP27
```

Сборка:

```bash
cmake --preset debug-picocalc-pico2w
cmake --build --preset lexxos-picocalc-pico2w
```

Профиль переносит PicoCalc LCD, southbridge keyboard/power/backlight service, USB CDC console mirror и SD wiring с
`picocalc_pico_w` на RP2350 target. Hardware smoke-test на реальном PicoCalc с Pico 2 W пройден 2026-07-08:
SD/FAT, LCD console, keyboard/southbridge snapshot, backlight/display-power, RTC/NTP, PSRAM, audio, Wi-Fi,
external `.lexx` runtime, reboot и `poweroff` подтверждены через UART.

Текущий preset использует RP2350/Pico 2 W runtime path; regression log показывает FreeRTOS task startup на
подтверждённой прошивке.

PicoCalc не должен оформляться как отдельный platform backend, если используется Pico SDK. Это другая плата, но не
другая platform abstraction.

## `m5stack_basic_v1`

Первый рабочий ESP32 board profile для ESP-IDF port.

```text
Processor: ESP32
Board id:  m5stack_basic_v1
Platform:  espidf
Console:   UART0
Display:   320x240 LCD on shared LCD/TF SPI bus
Input:     front buttons A/B/C
Storage:   TF/SD card over SPI, FatFs automount to /mnt/sd
```

Текущий bring-up поднимает LexxOS runtime поверх ESP-IDF scheduler, UART shell, GPIO backend, `/tmp`, LCD, кнопки,
SD block device, FatFs automount и диагностические команды `esp32-info` / `esp32-spi-test`.

## `jc_esp32p4_m3_dev`

ESP-IDF профиль для Guition / Jingcai `JC-ESP32P4-M3-DEV`. Профиль рассчитан на bring-up через UART shell и уже
включает 16 MB flash layout, 32 MB in-package PSRAM, SDIO/FAT, system I2C и базовый wired Ethernet.

```text
Processor: ESP32-P4NRW32 + ESP32-C6-MINI Wi-Fi/Bluetooth coprocessor
Board id:  jc_esp32p4_m3_dev
Platform:  espidf
Console:   UART0 GPIO37/GPIO38 via CH340C, 115200 baud
Chip rev:  ESP32-P4 v1.3 board verified; profile selects IDF rev<3 support, min v1.0
Flash:     16 MB QIO
PSRAM:     32 MB HEX PSRAM, ESP-IDF SPIRAM 200 MHz
Wireless:  ESP32-C6 SDIO hosted Wi-Fi 6 / Bluetooth 5, STA connect/status/autoconnect enabled
Storage:   TF/microSD over native SDMMC 4-bit, FatFs automount to /mnt/sd
I2C:       system/debug I2C0 GPIO7/GPIO8, 400 kHz
Display:   BTT Pi TFT70 V2.1 7" 800x480 over MIPI DSI
Ethernet:  IP101 RMII, PHY address 1, DHCP autostart
Network:   eth, wifi, ping, rtc-sync and software RTC auto-sync use the ESP-IDF network stack once DHCP is ready
RTC:       software RTC enabled as default; auto NTP refresh over Ethernet or WiFi; PCF8563/BM8563 probe remains available on I2C 0x51
```

Сборка и прошивка:

```bash
cmake --preset debug-jc-esp32p4-m3-dev
cmake --build --preset lexxos-jc-esp32p4-m3-dev
cmake --build --preset lexxos-jc-esp32p4-m3-dev-flash
```

То же через wrapper:

```bash
tools/lexxos-build jc-esp32p4 --flash
```

Перед прошивкой задайте `ESPPORT` для своего устройства и `ESPBAUD=115200`.
Ниже `XXXX` — условный идентификатор, замените его фактическим. Минимальный
терминальный доступ после прошивки:

```bash
screen /dev/cu.usbserial-XXXX 115200
```

Первые команды для smoke-test:

```text
version
board
devices
i2c-buses
i2c-scan system --quiet
storage
block-devs
display-info
console-info
display-fill blue
display-test
net-status
eth status
wifi-status
wifi-connect <ssid> <password>
wifi-config show
touch-info
touch-calibrate show
touch-draw -s 10
hosted-info
ping --count 1 <gateway-or-host>
rtc-config show
rtc-status
rtc-get
rtc-sync --source software --timeout 10000
rtc-status --source pcf8563 --bus system
esp32-info
```

Интерфейсная карта уже зафиксирована в
`src/lexxos/platform/espidf/board_jc_esp32p4_m3_dev.cpp`: JP1 expansion header, hosted ESP32-C6 SDIO,
IP101 RMII Ethernet, system I2C, ES8311/ES7210 audio nets, TF SDIO, MIPI DSI/CSI, USB OTG/USB-TTL, RS485 и battery ADC.
ESP32-C6 hosted WiFi включен через `esp_wifi_remote`/`esp_hosted`; `wifi-connect`, `wifi-status`, `net-status`, `ping`,
`wifi-scan`, автоподключение и software RTC auto-sync проверены на реальной плате после обновления C6 firmware до
ESP-Hosted 2.11.7. Audio,
camera, USB OTG, RS485 и battery ADC пока оставлены как комментарии/descriptor wiring для быстрого
последующего включения драйверов без риска случайно дёрнуть неподтверждённые периферийные линии.
MIPI DSI display включен для BIGTREETECH Pi TFT70 V2.1 / Raspberry Pi DSI-compatible 7" panel: LexxOS использует
RGB888 framebuffer с BGR element order, RGB888 DSI video, 1 data lane, 600 Mbps/lane, 25.979 MHz DPI clock и power/backlight control
через I2C `0x45`. `display-power timeout` хранит staged idle policy в `/mnt/sd/.lexx/display-power.conf`: default
уменьшает backlight до 10% через 5 минут и выключает экран ещё через 5 минут. Touch input зарегистрирован как
FocalTech/FT5x06-compatible `ft5x06-touch` на I2C `0x38`;
`touch-calibrate` сохраняет профиль в `/mnt/sd/.lexx/touch.conf`, а `touch-draw` проверяет одновременное рисование
несколькими касаниями. Wi-Fi autoconnect хранит credentials в `/mnt/sd/.lexx/wifi.conf`; legacy
`/mnt/sd/lexxos/wifi.conf` читается для миграции. LCD console mirror включен для shell/status output; `console-info`
на 800x480 показывает `133x57` символов и 24 px status bar.
Настройки software RTC/NTP хранятся в `/mnt/sd/.lexx/rtc.conf`: `rtc-config` управляет файлом напрямую, а успешный
`rtc-sync` сохраняет выбранные server/timeout/timezone/source. После boot post-mount handler восстанавливает RTC config,
затем touch calibration и Wi-Fi autoconnect.
Версия ESP-Hosted host component, C6 coprocessor firmware и app descriptor выводятся командами `hosted-info` и `board`.
Для обновления C6 без рабочей SD/FAT добавлена streaming-команда `hosted-ota-rx`.
Инструкция по сборке и обновлению C6 firmware: `docs/boards/jc_esp32p4_m3_dev.md`.
В публичных схемах отдельный RTC IC не подтверждён: `RTC_DAT/SDA1` и `RTC_CLK/SCL1` встречаются как touch-FPC
sideband labels. Поэтому профиль включает программный RTC (`--source software`) как default backend, а
`rtc-status --source pcf8563 --bus system` остается ручной проверкой внешнего/неподтвержденного PCF8563/BM8563-compatible
устройства на `0x51`.

## `m5stack_cardputer`

Профиль обычного M5Stack Cardputer на ESP32-S3, backend `espidf`, preset `debug-cardputer`.
Базовая feature matrix включает display и keyboard; не переносите сведения об SD, ES8311,
TCA8418 и BMI270 из профиля Adv на эту плату без проверки её wiring и backend.
Текущий hardware smoke для 0.1.0 ещё требуется.

## `m5stack_cardputer_adv`

Профиль для M5Stack Cardputer-Adv на ESP-IDF. Публичный preset — `debug-cardputer-adv`. Локальный user preset `user-espidf-m5stack_cardputer` используется как
bring-up preset для этой платы, но внутри выбирает board profile `m5stack_cardputer_adv`.

```text
Processor: ESP32-S3FN8 / Stamp-S3A
Board id:  m5stack_cardputer_adv
Platform:  espidf
Console:   USB-Serial/JTAG, 115200 baud shell
Display:   ST7789V2-compatible 240x135 RGB565 LCD on SPI
Input:     TCA8418 keyboard on I2C
Storage:   microSD over SPI, FatFs automount to /mnt/sd
Audio:     ES8311 codec over I2C + I2S1, speaker output and microphone input
Motion:    BMI270 on I2C address 0x69
IR:        transmitter on GPIO44, 38 kHz carrier
Game Boy:  enabled in the local user preset with mapped flash staging and dynamic 8Kx6 ROM cache
Battery:   ADC voltage estimate on GPIO10, status bar percent estimate
```

Основные проверочные команды:

```text
board
devices
display-info
display-test
storage
block-devs
fs-info
mount
keyboard-live -s 2 -i 100
keyboard-raw -s 2 -i 100
gamepad-live -s 2 -i 100
imu -n 8 -i 100
ir info
ir samsung-power -r 1
audio
beep --frequency 880 --duration 80 --volume 96
mplay --test
mic info
mic sample
mic loopback -s 1 -g 2
gb-info /mnt/sd/gb/test-rom-2.gb
gbemu --profile --frames 60 --display none /mnt/sd/gb/test-rom-2.gb
gbemu --audio --profile --frames 600 --display none /mnt/sd/gb/test-rom-2.gb
power
console-info
```

Cardputer-Adv keyboard uses the TCA8418 controller event FIFO. LexxOS filters unused controller columns/rows, applies a
small software debounce window and resets local key state after controller FIFO overflow, so idle `keyboard-live` /
`keyboard-raw` should not report random key activity even while the USB Serial/JTAG console is connected. Manual typing
is still the final check for layout-level issues.

Cardputer-Adv exposes the built-in BMI270 as `cardputer-bmi270` through the `sensor` device registry and the `imu`
diagnostic command. The BMI270 requires a Bosch config file upload during initialization; after successful load,
`imu` prints raw accel/gyro/temperature samples. The board also exposes `cardputer-ir` as an IR transmitter device on
GPIO44. `ir info` prints the pin/carrier, and `ir samsung-power` sends the common Samsung BN59-class power toggle frame
`0xE0E040BF`; `ir samsung-on`, `ir samsung-off` and `ir samsung-code -c <hex>` are available for manual line-of-sight
testing.

Expansion pins are listed as user GPIO in `board`: HY2.0 `G1/G2` and EXT `G3/G4/G5/G6` are visible input/output pins.
Shared EXT pins for UART, SD and I2C (`G13/G15/G39/G40/G14/G8/G9`) are present in the descriptor but hidden by default
or output-disabled, so programs can discover them without accidentally driving active board buses.

`audio` and portable `beep` use the ES8311 codec at I2C address `0x18` and I2S1 pins `BCLK=41`, `WS=43`, `DOUT=42`,
`DIN=46`. The current bring-up exposes tone output and blocking PCM buffer output for `mplay`; tone mixing is still
reported as unavailable. Cardputer-Adv uses `44100 Hz` native PCM output, stereo 16-bit I2S framing and 8x256 DMA
frames. `mplay --test` should report `441000` PCM frames and finish in about 10-11 seconds; a much longer run usually
means foreground cancellation polling is blocking between audio buffers.

`mic` uses the ES8311/I2S RX path and registers `cardputer-adv-mic`. `mic sample` captures one PCM chunk and prints
peak/average values; `mic loopback -s 1 -g 2` captures microphone input and immediately plays it through the speaker.

`gbemu` is enabled in `user-espidf-m5stack_cardputer` with no PSRAM requirement on ESP-IDF. The preset uses
`LEXXOS_GAMEBOY_ROM_CACHE_BANKS=3`, `LEXXOS_GAMEBOY_ROM_CACHE_PAGE_BYTES=8192` and dynamic ROM cache allocation, which
produces the runtime diagnostic `rom-cache=8Kx6=48K` while keeping the cache out of static `.bss`. Playable runs default
to mapped flash staging in the ESP-IDF `storage` partition, so successful runs print `rom-storage=flash` and
`cache-fills=0` after the ROM has been staged. Direct shell runs keep ESP-IDF audio disabled unless `--audio` is passed;
the TUI launcher uses `[audio] enabled = true` by default and passes `--audio` for selected ROMs. The keyboard-backed
`cardputer-gamepad` maps `e/z/a/d` to up/down/left/right, `[`/`]` to A/B, `;` to X and `` ` `` to Y; Start is Enter
and Select is Backspace.

`clear` uses the shared console clear path and clears the full LCD text area below the status bar, not only the logical
character cells. This avoids stale pixels in the right/bottom margins on narrow text layouts.

Ограничение питания: Cardputer и Cardputer-Adv аппаратно не предоставляют LexxOS достоверный признак текущей зарядки
или ток батареи. LexxOS читает только напряжение через ADC GPIO10 и показывает процент как приблизительную оценку.
Поэтому `power` печатает `charging=unavailable`, а статус-бар отображает уровень батареи без charging icon. Для
зарядки по документации M5Stack боковой выключатель должен быть в положении `ON`.

## `euse_m4_demo_stm32f407`

Реализованный порт для `STM32F407ZGT6 Euse M4 DEMO Large`, backend `stm32f4`.
Публичные presets: `debug-stm32f407` и `debug-stm32f407-full`.

```text
Processor: STM32F407ZGT6 / Cortex-M4F
Board id:  euse_m4_demo_stm32f407
Debug:     SWD PA13/PA14
Console:   USART1 PA9/PA10, 115200
Display:   EU-TFT3.2-320x240 on DIS header, parallel 16-bit FSMC/8080-style bus
```

В порте есть startup/linker, clock/UART, FreeRTOS runtime, parallel TFT и SDIO/FatFs.
Режим `LEXXOS_STM32F4_BRINGUP_ONLY=ON` оставляет минимальную bring-up прошивку.
Сборка не подтверждает работу периферии на текущем release candidate.

- [Первичный запуск](stm32f407-bringup.md)
- [Дисплей и pinout](stm32f407-display.md)
- [SDIO](stm32f407-sdio.md) и [проверка надёжности](stm32f407-sdio-reliability.md)
- [CLion/OpenOCD](stm32f407-clion-macos.md)

## Debug OLED

Для development profiles LexxOS рассматривает OLED как необязательную диагностическую периферию.

SSD1306 driver находится в:

```text
src/lexxos/devices/display/
```

Pico I2C backend находится в:

```text
src/lexxos/devices/pico/
```

Если OLED отвечает по адресу `0x3C`, firmware показывает диагностический экран:

```text
LEXXOS DEBUG
<board id>
CPU <processor>
UP <milliseconds> MS
HEAP <bytes> B
TASKS <count>
UART<index> <baud>
SD <state>
```

Если экран не подключён или не отвечает, загрузка продолжается через UART и LED diagnostics.

## SD-карта

Development profiles используют SD interface из board descriptor и portable backend-слой:

```text
SdCardBackend
  SdSpiBackend   для `dev_pico`
  SdSdioBackend  для STM32F407 и будущих SDIO-профилей
```

Карта необязательна при boot. Если инициализация прошла успешно, устройство регистрируется как block device и доступно в
VFS:

```text
/dev/sd0
```

Default FAT mount point:

```text
/mnt/sd
```

Основные shell-команды для bring-up и диагностики:

```text
storage
storage probe
storage read0
storage diag
block-devs
mount
mount /dev/sd0 /mnt/sd
umount /mnt/sd
ls /dev
stat /dev/sd0
cat /sys/storage
```

Для `dev_pico` нормальный workflow автоматический:

```text
sd card: inserted
sd automount: probing (inserted)
sd automount: mounted /dev/sd0 on /mnt/sd type fat
```

FatFs backend на текущем этапе поддерживает FAT32 read/write и ASCII LFN. Runtime repair/fsck command не входит в
firmware; для проверки целостности используйте host-инструменты на вынутой SD-карте.

Текущий production-профиль SD/FAT для `dev_pico`:

```text
sd board interface: spi
sd interface: spi
active baud: 50000000 Hz
CMD18 read: on
CMD25 write: off
FAT read cache: 16 sectors
FAT write buffer: 8 sectors
FAT disk write max sectors: 8
```

Подробности, известные ограничения `CMD25` и regression checklist находятся в `docs/sd-card-production.md`.

## SD card-detect

Hot-plug использует GP22 как active-low card-detect. На Pico изменение уровня фиксируется GPIO interrupt, а обработка
выполняется отложенно в общей задаче `system-events`: debounce, SD probe, mount/unmount и console output не выполняются
из ISR.

Для platform backend без GPIO interrupt предусмотрен polling fallback внутри `SystemEventService`.

Подробная схема подключения описана в `docs/sd-card-detect.md`.

## ESP-IDF backend

ESP-IDF уже реализован. Доступные board profiles: `m5stack_basic_v1`, `m5stack_cardputer`,
`m5stack_cardputer_adv`, `jc_esp32p4_m3_dev`. Набор драйверов и capabilities различается по платам.
Профиль `esp32_devkit` не зарегистрирован. Сборка запускается из корня репозитория;
подробности в [инструкции сборки](build.md).
