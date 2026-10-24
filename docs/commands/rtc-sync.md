# rtc-sync

Синхронизирует выбранный RTC backend с NTP сервером через активный сетевой интерфейс.

## Синтаксис

```text
rtc-sync [--server <host>] [--timeout <ms>] [--tz <offset>] [--source auto|software|picocalc|ds3231|pcf8563] [--bus <name>] [--address <addr>]
```

## Параметры

- `--server <host>` / `-n <host>` — NTP сервер; по умолчанию значение из `/mnt/sd/.lexx/rtc.conf` или `pool.ntp.org`.
- `--timeout <ms>` / `-t <ms>` — общий таймаут DNS + NTP; по умолчанию значение из `/mnt/sd/.lexx/rtc.conf` или `8000`,
  максимум `60000`.
- `--tz <offset>` / `-z <offset>` — фиксированное смещение часового пояса перед записью в RTC; по умолчанию значение из
  `/mnt/sd/.lexx/rtc.conf` или `UTC`.
  Поддерживаются `UTC`, `Z`, `+HH`, `+HH:MM`, `+HHMM`, `-HH`, `-HH:MM`, `-HHMM`.
- `--source auto|software|picocalc|ds3231|pcf8563` / `-s ...` — RTC backend; `bm8563` принимается как алиас `pcf8563`;
  по умолчанию значение из `/mnt/sd/.lexx/rtc.conf` или `auto`.
- `--bus <name>` / `-b <name>` — I2C bus для внешнего RTC; по умолчанию значение из `/mnt/sd/.lexx/rtc.conf` или `debug`.
- `--address <addr>` / `-a <addr>` — адрес внешнего RTC; если не задан, используется адрес backend-а: `0x68` для DS3231, `0x51` для PCF8563/BM8563.

## Пример

```text
lexx:/> wifi-connect MyNetwork mypassword
lexx:/> rtc-sync
rtc-sync: resolving pool.ntp.org
rtc-sync: sending NTP request
source: picocalc-southbridge
network time: 2026-06-28 12:34:56 UTC
rtc time: 2026-06-28 12:34:56 UTC
unix: 1782640496
rtc sync: ok
```

С записью локального времени в RTC:

```text
lexx:/> rtc-sync --tz +07:00
rtc-sync: resolving pool.ntp.org
rtc-sync: sending NTP request
source: picocalc-southbridge
network time: 2026-06-28 12:34:56 UTC
rtc time: 2026-06-28 19:34:56 UTC+07:00
unix: 1782640496
rtc sync: ok
```

После успешной синхронизации эффективные параметры сохраняются в `/mnt/sd/.lexx/rtc.conf`. Например команда выше
запишет `tz=+07:00`, и следующие `rtc-sync`/boot auto-sync будут использовать это смещение без повторного `--tz`.

## Замечания

- NTP всегда возвращает UTC. Параметр `--tz` применяет только фиксированное смещение перед записью в RTC.
- Автоматический переход на летнее/зимнее время пока не выполняется; при необходимости указывайте актуальное смещение вручную.
- Перед запуском нужен поднятый сетевой интерфейс: Wi-Fi на Pico W или DHCP-ready Ethernet на ESP-IDF платах.
- Платы могут включать `software` RTC backend и автоматическую NTP-синхронизацию в board profile; тогда `auto`
  выбирает программный RTC согласно настройке платы или сохраненному `/mnt/sd/.lexx/rtc.conf`.
- Для ручного управления файлом используйте `rtc-config show`, `rtc-config set` и `rtc-config clear`.
- На сборках без сетевого NTP backend команда сообщит, что NTP недоступен.
