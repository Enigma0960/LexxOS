# ping

Проверяет реальное IP-соединение через ICMP echo request/reply на активном сетевом интерфейсе.

На PicoCalc/Pico W команда использует прежний CYW43/lwIP RAW path. На ESP-IDF сборках используется стандартный
raw ICMP socket поверх активного интерфейса, например Ethernet на `jc_esp32p4_m3_dev`.

## Синтаксис

```text
ping [--count <n>] [--timeout <ms>] <host-or-ip>
```

## Параметры

- `--count <n>` / `-c <n>` — число ICMP-запросов; по умолчанию `4`, максимум `10`.
- `--timeout <ms>` / `-t <ms>` — таймаут одного запроса и DNS resolve; по умолчанию `5000`, максимум `60000`.
- `<host-or-ip>` — IPv4-адрес или DNS-имя.

## Примеры

```text
lexx:/> ping 8.8.8.8
PING 8.8.8.8 (8.8.8.8): 32 data bytes
32 bytes from 8.8.8.8: icmp_seq=1 time=34 ms
32 bytes from 8.8.8.8: icmp_seq=2 time=31 ms
--- ping statistics ---
2 packets transmitted, 2 received
```

```text
lexx:/> ping --count 1 pool.ntp.org
ping: resolving pool.ntp.org
PING pool.ntp.org (192.0.2.123): 32 data bytes
```

Перед запуском нужен поднятый сетевой интерфейс: Wi-Fi через `wifi-connect`/автоподключение на Pico W или DHCP-ready
Ethernet на ESP-IDF платах (`eth status`).

Во время ожидания ответа `Ctrl+C` запрашивает cooperative cancellation и останавливает команду после текущей
короткой итерации ожидания.
