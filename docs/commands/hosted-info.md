# `hosted-info`

Показывает информацию об ESP-Hosted coprocessor, если активная плата использует ESP32-C6/ESP-Hosted для Wi-Fi.

На `jc_esp32p4_m3_dev` команда поднимает ESP-Hosted SDIO backend, затем читает версию host component, версию прошивки
ESP32-C6 и app descriptor slave-прошивки. Это быстрый способ проверить, какая C6-прошивка сейчас стоит на плате и
совпадает ли она с host-компонентом, собранным в LexxOS.

## Синтаксис

```text
hosted-info [--no-start]
```

## Параметры

- `--no-start` - не запускать Wi-Fi/ESP-Hosted backend перед опросом. Полезно, если нужно посмотреть, поднят ли
  transport уже сейчас, без side effects.

## Вывод

```text
supported: yes
backend: esp-hosted-sdio
radio: ESP32-C6-MINI
transport: sdio index=1 clk=GPIO18 cmd=GPIO19 d0=GPIO14 d1=GPIO15 d2=GPIO16 d3=GPIO17 xfer-hz=40000000
reset: GPIO54
backend-start-requested: yes
backend-started: yes error=ESP_OK
host-version: 2.11.7
coprocessor-version: 2.11.7 error=ESP_OK
major-minor-compatible: yes
version-order: same
activate-supported: yes
app-desc: yes error=ESP_OK
app-name: network_adapter
app-version: 2.11.7
app-idf: v6.0.2
app-date: ...
app-time: ...
secure-version: 0
```

На некоторых C6 сборках ESP-Hosted отдает только basic descriptor (`app-name`, `app-version`, `app-idf`) без full
descriptor magic/date/time. Такой ответ всё равно считается валидным, если basic fields заполнены.

Если backend не был запущен и передан `--no-start`, поля coprocessor/app обычно будут с
`error=ESP_ERR_INVALID_STATE`. Это ожидаемо: для RPC-запросов к C6 нужен поднятый ESP-Hosted transport.

## Связанные команды

- `board` - выводит короткую строку `hosted:` и `hosted/app:` без принудительного запуска backend.
- `wifi-status`, `wifi-connect`, `net-status` - используют тот же board-level Wi-Fi/network backend.

## Доступность

Доступна в ESP-IDF сборках, где активный board descriptor описывает Wi-Fi backend `esp-hosted-*`. Документ
соответствует LexxOS 0.0.7.
