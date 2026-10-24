# `eth`

## Исходник

`src/lexxos/apps/network/ethernet_program.cpp`

## Подробное описание

Показывает и управляет wired Ethernet интерфейсом, который описан активным ESP-IDF board descriptor. На
`jc_esp32p4_m3_dev` команда работает с IP101 RMII PHY и показывает link, MAC, speed/duplex и DHCP IPv4 параметры.
DHCP hostname формируется как `lexxos-XXXX`, где `XXXX` — последние два байта MAC-адреса в uppercase hex.

## Параметры

- `status` — показать текущее состояние; действие по умолчанию.
- `start` — запустить Ethernet driver и netif.
- `stop` — остановить Ethernet driver.
- `restart` — остановить и снова запустить Ethernet driver.

## Примеры

```text
lexx:/> eth status
lexx:/> eth restart
```

## Справка shell

```text
eth - show or control wired Ethernet

Starts, stops and reports the ESP-IDF Ethernet interface registered by the active board descriptor.

Usage:
  eth [status|start|stop|restart]
```

## Доступность

Доступна в ESP-IDF сборках, где активный board descriptor описывает wired Ethernet. Документ соответствует LexxOS
0.0.7.
