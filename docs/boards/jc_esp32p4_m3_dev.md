# `jc_esp32p4_m3_dev`

Практические заметки по bring-up Guition / Jingcai `JC-ESP32P4-M3-DEV`.

## ESP32-C6 coprocessor

Плата использует ESP32-C6-MINI как Wi-Fi/Bluetooth coprocessor для ESP32-P4. P4 общается с C6 через ESP-Hosted SDIO:

```text
SDIO CLK: GPIO18
SDIO CMD: GPIO19
SDIO D0:  GPIO14
SDIO D1:  GPIO15
SDIO D2:  GPIO16
SDIO D3:  GPIO17
C6 reset: GPIO54
```

Проверить текущую прошивку C6:

```text
hosted-info
```

Короткая строка также есть в общей информации о плате:

```text
board
```

`board` не стартует ESP-Hosted принудительно. Если Wi-Fi backend ещё не поднят, это нормальный вывод:

```text
hosted: host-version=2.11.7 coprocessor-version=- version-error=ESP_ERR_INVALID_STATE compatible=unknown app-desc=no app-error=ESP_ERR_INVALID_STATE
```

После `hosted-info`, `wifi-status` или `wifi-connect` transport уже поднят, и `board` может показать app descriptor C6.

## SD card и Wi-Fi одновременно

На ESP32-P4 microSD и ESP32-C6 SDIO физически могут работать вместе: у SoC один SDMMC host controller, но два SDIO
slot-а. На этой плате разводка такая:

```text
microSD:  Slot 0, CLK=GPIO43 CMD=GPIO44 D0..D3=GPIO39..GPIO42
ESP32-C6: Slot 1, CLK=GPIO18 CMD=GPIO19 D0..D3=GPIO14..GPIO17 reset=GPIO54
```

Проблема была не в разводке, а в software ownership: SD backend и ESP-Hosted оба пытались отдельно вызвать
`sdmmc_host_init()` для одного и того же host controller. В LexxOS профиль `jc_esp32p4_m3_dev` снова собирается с
ESP-Hosted/Wi-Fi, а SDIO backend умеет делить общий SDMMC host с ESP-Hosted: если Hosted уже поднял controller, SD
инициализирует только Slot 0 и при освобождении трогает только свой slot.

Проверочный сценарий на реальной плате:

```text
hosted-info
wifi-status
storage
ls /mnt/sd
```

Ожидаемый результат: `hosted-info` показывает ESP-Hosted 2.11.7, `wifi-status` показывает `initialized: yes` и
`started: yes`, а `storage` остаётся `sd state: ready`.

## Обновления через SD staging

Теперь Wi-Fi и SD можно использовать как нормальный двухэтапный update path:

1. Wi-Fi скачивает payload во временный файл на SD, например `/mnt/sd/.updates/firmware/<version>/image.bin.tmp`.
2. После полного скачивания LexxOS проверяет размер, hash и подпись/manifest, затем атомарно переименовывает `.tmp` в
   готовый payload.
3. Firmware update устанавливается уже из SD в OTA/app partition, с rollback marker до подтверждения успешной загрузки.
4. `.lexx` программы обновляются тем же staging-подходом: download в `.tmp`, проверка manifest/hash, затем atomic rename
   поверх версии в `/mnt/sd/apps` или отдельном каталоге пакетов.

Это разделяет сетевой риск и риск установки: если Wi-Fi оборвался, текущая firmware и текущие `.lexx` файлы не
трогаются; если payload уже проверен на SD, установка может идти без сети.

## Сборка C6 slave firmware

Для LexxOS, собранной с ESP-IDF 6.0.2 и component `espressif/esp_hosted` 2.11.7, slave firmware лучше собирать из
того же managed component, который лежит в workspace. `/path/to/esp-idf` замените
путём установленного SDK, `/path/to/esp-idf-python` — путём Python из активированного
ESP-IDF environment (проверьте `command -v python` после `export.sh`). Команды
выполняются из корня LexxOS:

```bash
source /path/to/esp-idf/export.sh
cmake -S managed_components/espressif__esp_hosted/slave -B build/esp-hosted-c6-slave -G Ninja \
  -DIDF_TARGET=esp32c6 \
  -DPYTHON=/path/to/esp-idf-python \
  -DPYTHON_DEPS_CHECKED=1 \
  -DCCACHE_ENABLE=False
cmake --build build/esp-hosted-c6-slave
```

Ожидаемый app binary:

```text
build/esp-hosted-c6-slave/network_adapter.bin
```

## Обновление C6 через ESP-Hosted OTA

Рекомендуемый путь - OTA через уже подключенный SDIO transport. Официальный пример Espressif
`host_performs_slave_ota` делает именно это: P4/host читает binary из LittleFS, partition, HTTPS, SPIFFS, SD, NVS или
custom storage и передает C6 chunks через `esp_hosted_slave_ota_begin/write/end/activate`.

Практический LexxOS workflow:

```text
hosted-info
yrx /mnt/sd/network_adapter.bin
hosted-ota --dry-run /mnt/sd/network_adapter.bin
hosted-ota --reboot-host /mnt/sd/network_adapter.bin
hosted-info
```

Если SD-карта ещё не готова (`storage` показывает `sd state: no-bus` или `/mnt/sd` не монтируется), используйте
streaming OTA через UART:

```text
hosted-info
hosted-ota-rx --dry-run
hosted-ota-rx --reboot-host
hosted-info
```

На host computer файл отправляется YMODEM sender-ом, например через `tools/lexxcp/lexxcp/ymodem.py`.

Если текущая C6 firmware поддерживает `OTAActivate` (ESP-Hosted 2.6.0+), `hosted-ota` активирует новую прошивку через
RPC. На более старых C6 firmware, например 2.3.x, `activate` будет пропущен: старый OTA path завершает обновление на
`end`, после чего host нужно перезагрузить, чтобы ESP-Hosted transport заново синхронизировался с C6.

## Прямое обновление C6 по UART

Этот путь нужен как recovery, если hosted OTA недоступен. Нужен ESP-Prog или USB-UART адаптер, подключенный к C6 JP1 /
`PROG_C6` линиям. Не подключайте внешнее питание 3.3 V, если плата уже питается от USB.

Официальная схема для ESP-Prog:

```text
ESP_EN  -> EN
ESP_TXD -> TXD
ESP_RXD -> RXD
GND     -> GND
ESP_IO0 -> IO0
```

Для `JC-ESP32P4-M3-DEV` полезные C6 линии в board profile:

```text
JP1 pin 20: C6 U0RXD
JP1 pin 22: C6 U0TXD
JP1 pin 24: C6 IO9 / boot strap
JP1 pin 26: C6 CHIP_PU
```

Команды прошивки после сборки C6 slave:

```bash
source /path/to/esp-idf/export.sh
ESPPORT=<c6-uart-port> cmake --build build/esp-hosted-c6-slave --target flash
idf.py -B build/esp-hosted-c6-slave -p <c6-uart-port> monitor
```

После обновления C6:

```text
hosted-info
wifi-scan --timeout 15000 --limit 8 --bssid
wifi-status
wifi-connect <ssid> <password>
net-status
```

## Wi-Fi scan

Официальный `wifi_scan` demo из репозитория платы делает минимальную последовательность:

```text
esp_event_loop_create_default()
esp_wifi_init()
esp_wifi_set_mode(WIFI_MODE_STA)
esp_wifi_start()
esp_wifi_scan_start(NULL, true)
```

В LexxOS используется тот же ESP-IDF/ESP-Hosted Wi-Fi API, но команда запускается как универсальная shell-задача рядом с
Ethernet, RTC, VFS и автоподключением. Поэтому для `jc_esp32p4_m3_dev` оставлены две важные настройки:

- `wifi-scan` имеет stack `8192` bytes, потому что ESP-Hosted RPC/protobuf path заметно тяжелее Pico/CYW43 scan.
- ESP-IDF backend передает C6 явный active scan config вместо `NULL` default config.
- `CONFIG_ESP_HOSTED_WIFI_AUTO_CONNECT_ON_STA_START` выключен; подключением управляет LexxOS `wifi-connect` и
  `wifi-autoconnect`, а не неявный Hosted connect сразу на `STA_START`.
- `wifi-config` сохраняет credentials в `/mnt/sd/.lexx/wifi.conf`; старый `/mnt/sd/lexxos/wifi.conf` читается как
  legacy-конфигурация и мигрируется в новый путь.
- На этой ESP-IDF плате SD монтируется через runtime event task, поэтому `wifi-autoconnect` вызывается и boot task-ом,
  и post-mount handler-ом после успешного mount `/mnt/sd`. Общий guard не дает запустить две параллельные попытки.
- Post-mount handler применяет сохраненные system configs в порядке: `/mnt/sd/.lexx/rtc.conf`,
  `/mnt/sd/.lexx/touch.conf`, затем Wi-Fi autoconnect.

Проверено на реальной плате после обновления C6 до ESP-Hosted 2.11.7:

```text
wifi-scan --timeout 15000 --limit 5 --bssid
networks: 5
wifi-connect <ssid> <password>
wifi-status
ping --count 1 <gateway>
```

## BTT Pi TFT70 V2.1 MIPI DSI

Профиль платы включает `btt-tft70-dsi` как основной `DisplayDevice`:

```text
display-info
display-fill blue
display-test
console-info
```

Текущий режим рассчитан на Raspberry Pi DSI-compatible 7" 800x480 panel:

```text
DSI:       lane0 + clock, 600 Mbps/lane
DPI clock: 25.979 MHz
Timing:    HFP=210 HSW=2 HBP=46, VFP=22 VSW=20 VBP=4
Memory:    RGB888 framebuffer in PSRAM
Video:     LexxOS RGB565 API converted to BGR888 framebuffer / RGB888 over DSI
Power:     MIPI DSI PHY internal LDO3 at 2.5 V
Control:   I2C 0x45, PWM/backlight register 0x86
```

Экранная shell-консоль включена через общий LCD console mirror. После boot banner и prompt дублируются на TFT70; для
800x480 текущая текстовая геометрия `133x57` символов плюс 24 px status bar. Диагностика:

```text
console-info
clear
backlight
display-power
power
```

`display-power timeout` хранит настройки автоотключения в общем каталоге системных конфигураций:

```text
/mnt/sd/.lexx/display-power.conf
```

Если файла нет, используется staged default: через 300 секунд простоя LCD backlight уменьшается до `26` из `255`
примерно 10%, ещё через 300 секунд экран выключается. Любой UART/USB keyboard input или touch activity будит экран и
сбрасывает idle timer. `backlight lcd <0..255>` позволяет вручную проверить яркость.

## Touch input

Текущая TFT70 ревизия отвечает на `system` I2C sideband (`GPIO7/GPIO8`) по адресу `0x38`, поэтому профиль платы
регистрирует FocalTech/FT5x06-compatible контроллер как `ft5x06-touch`:

```text
devices
touch-info
touch-calibrate show
```

Калибровка рисует пять мишеней и сохраняет платформозависимый профиль на SD:

```text
touch-calibrate
```

Файл калибровки:

```text
/mnt/sd/.lexx/touch.conf
```

Если файл существует, LexxOS применяет его после mount `/mnt/sd` на этапе загрузки. Тест рисования:

```text
touch-draw
touch-draw -s 30 -r 4
```

`touch-draw` ведет отдельный цветной stroke для каждого active contact id, поэтому подходит для проверки single-touch и
multi-touch. Если эта BTT revision не показывает pixels, первые быстрые ручки профиля остаются `dataLanes`,
`laneBitRateMbps` и DSI timing block в `src/lexxos/platform/espidf/board_jc_esp32p4_m3_dev.cpp`.

## RTC/NTP settings

Профиль `jc_esp32p4_m3_dev` использует software RTC как default backend. Настройки auto-sync, NTP server, timeout,
timezone и default `rtc-sync` source хранятся на SD:

```text
/mnt/sd/.lexx/rtc.conf
```

Ручная настройка:

```text
rtc-config set --tz +07:00 --server pool.ntp.org --timeout 12000 --source software --auto-sync yes
rtc-config show
rtc-sync
rtc-status --source software
```

После успешного mount `/mnt/sd` boot runtime загружает `rtc.conf`, применяет его к software RTC task и выводит
`rtc-config: restored path=/mnt/sd/.lexx/rtc.conf`. Успешный `rtc-sync --tz +07:00` также обновляет этот файл, поэтому
смещение часового пояса и NTP параметры переживают reboot.

## ES8311 audio / speaker

Профиль платы включает основной `AudioDevice` через ES8311 на I2C `0x18` и I2S0:

```text
MCLK: GPIO13
BCLK: GPIO12
WS:   GPIO10
DOUT: GPIO9
DIN:  GPIO48
PA:   GPIO11 active-high
```

Быстрая проверка:

```text
audio
beep --frequency 880 --duration 250 --volume 128
mplay --test
```

## Источники

- Espressif ESP32-P4 Function EV board guide:
  `https://github.com/espressif/esp-hosted-mcu/blob/main/docs/esp32_p4_function_ev_board.md`
- JC-ESP32P4-M3-DEV community schematic bundle:
  `https://github.com/DRubioG/JC-ESP32P4-M3-DEV/blob/main/5-Schematic/schematic.pdf`
- ESP32-P4 ES8311 Arduino discussion with JC pin notes:
  `https://github.com/espressif/arduino-esp32/discussions/12434`
- Espressif `host_performs_slave_ota` example:
  `https://components.espressif.com/components/espressif/esp_hosted/versions/2.11.7/examples/host_performs_slave_ota?language=en`
- Espressif MIPI DSI LCD API:
  `https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/peripherals/lcd/dsi_lcd.html`
- BIGTREETECH Pi TFT70 V2.0 hardware manual:
  `https://github.com/bigtreetech/BIGTREETECH-TouchScreenHardware/blob/master/BTT%20Pi%20TFT70%20V2.0%20Github/Hardware/BTT-Pi%20TFT70%20user%20manual.pdf`
