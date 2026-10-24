# net-status

Показывает состояние сетевых интерфейсов через общий сетевой backend платы.

На ESP-IDF сборках выводит Ethernet и WiFi, если оба объявлены профилем платы. Для каждого интерфейса печатаются link,
MAC, hostname, IP, netmask, gateway и последняя ошибка; Ethernet дополнительно показывает speed/duplex, WiFi — SSID,
RSSI и DNS. На Pico W команда использует общий WiFi status path.

## Синтаксис

```text
net-status
```

## Примеры

Адреса, hostname и идентификаторы ниже условные. Сеть `192.0.2.0/24` используется
только для документации; для реальных команд берите параметры своего подключения.

```text
lexx:/> net-status
interface: ethernet
supported: yes
initialized: yes
started: yes
link: up
hostname: lexxos-ethernet-example
ip: 192.0.2.10
gateway: 192.0.2.1

interface: wifi
supported: yes
initialized: yes
started: yes
link: up
hostname: lexxos-wifi-example
ssid: MyNetwork
ip: 192.0.2.11
```

Для ESP-IDF Ethernet-specific управления остаётся команда `eth status|start|stop|restart`.
