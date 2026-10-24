# `wifi-scan`

## Подробное описание

Печатает список видимых Wi-Fi сетей через board-level WiFi backend. На Pico/CYW43 по умолчанию одинаковые записи
`SSID + channel + auth` объединяются, а сырой режим показывает исходные записи сканирования CYW43. На ESP-IDF backend
команда использует `esp_wifi_scan_*`, если профиль платы явно разрешает scan.

## Параметры

- `--timeout <ms>` / `-t <ms>` — необязательное время ожидания; по умолчанию `10000`; диапазон `1..120000`.
- `--limit <count>` / `-n <count>` — необязательный лимит результатов; по умолчанию `24`; диапазон `1..32`.
- `--raw` / `-r` — необязательный флаг; отключает объединение повторов и всегда печатает BSSID.
- `--bssid` / `-b` — необязательный флаг; печатает BSSID для объединённого результата.

## Примеры

```text
lexx:/> wifi-scan
wifi-scan: scanning...
networks: 5 merged=14

lexx:/> wifi-scan --raw
wifi-scan: scanning...
networks: 24 dropped=1
```

## Комментарий автора

Подтверждено на реальном ClockworkPi PicoCalc / Pico W: сканирование завершилось, обычный режим объединил повторы, а
сырой режим показал записи BSSID/channel/RSSI/auth. На `jc_esp32p4_m3_dev` scan подтверждён через ESP32-C6
ESP-Hosted 2.11.7: команда использует явный active scan config и увеличенный stack, потому что Hosted RPC путь тяжелее,
чем прямой Wi-Fi backend Pico/CYW43.

## Справка shell

```text
wifi-scan - scan Wi-Fi networks

Starts the board Wi-Fi station backend and prints visible Wi-Fi networks.

Usage:
  wifi-scan [--timeout <ms>] [--limit <count>] [--raw] [--bssid]

Arguments:
  -t, --timeout <ms> default=10000 - maximum time to wait for scan completion
  -n, --limit <count> default=24 - maximum network results to print
  -r, --raw - print raw backend scan records
  -b, --bssid - print access point BSSID
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы и поля `board.wireless.scan`.
Документ соответствует LexxOS 0.0.7.
