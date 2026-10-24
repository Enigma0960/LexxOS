# wifi-status

Показывает состояние активного Wi-Fi STA-интерфейса через board-level WiFi backend.

## Синтаксис

```text
wifi-status
```

## Пример

Адреса, hostname и идентификаторы ниже условные. Сеть `192.0.2.0/24` используется
только для документации; для реальных команд берите параметры своего подключения.

```text
lexx:/> wifi-status
interface: wifi
supported: yes
initialized: yes
sta-mode: enabled
started: yes
link: up
mac: 02:00:00:00:00:01
hostname: lexxos-wifi-example
ssid: MyNetwork
rssi: -63 dBm level=2
ip: 192.0.2.10
netmask: 255.255.255.0
gateway: 192.0.2.1
dns0: 192.0.2.1
dns1: 0.0.0.0
```

## Замечания

- `link=up` означает, что Wi-Fi подключён и DHCP уже выдал IPv4-адрес.
- `link=no-ip` означает, что ассоциация с точкой доступа прошла, но IP-адрес ещё не получен.
- `ip: 0.0.0.0` при `link=no-ip` указывает на проблему DHCP.
- На ESP-IDF hostname формируется как `lexxos-XXXX`, где `XXXX` — последние два байта MAC интерфейса в uppercase hex.
