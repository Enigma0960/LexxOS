# `hosted-ota`

Обновляет ESP32-C6 ESP-Hosted slave firmware из binary-файла в VFS.

Команда предназначена для плат, где Wi-Fi/Bluetooth живут на отдельном ESP32-C6 coprocessor и подключены к host MCU
через ESP-Hosted SDIO/SPI/UART. На `jc_esp32p4_m3_dev` используется SDIO transport.

## Синтаксис

```text
hosted-ota [--dry-run] [--no-activate] [--reboot-host] <path>
```

## Параметры

- `<path>` - путь к ESP32-C6 `network_adapter.bin` в VFS, обычно `/mnt/sd/network_adapter.bin`.
- `--dry-run` - проверить файл, ESP image magic и ESP-Hosted transport, но не писать flash C6.
- `--no-activate` - пропустить `esp_hosted_slave_ota_activate()`, даже если текущая C6-прошивка его поддерживает.
- `--reboot-host` - перезагрузить ESP32-P4 после успешной OTA. Полезно, чтобы пересинхронизировать SDIO transport.

## Типовой workflow

```text
hosted-info
yrx /mnt/sd/network_adapter.bin
hosted-ota --dry-run /mnt/sd/network_adapter.bin
hosted-ota --reboot-host /mnt/sd/network_adapter.bin
```

Если SD/FAT ещё не подняты, используйте `hosted-ota-rx`: она принимает тот же `network_adapter.bin` по UART/YMODEM и
пишет его в C6 OTA без промежуточного файла.

После перезагрузки:

```text
hosted-info
wifi-status
```

Если текущая C6-прошивка старее ESP-Hosted 2.6.0, команда не вызывает `activate`, потому что такого RPC ещё нет.
Официальный ESP-Hosted OTA example считает этот случай допустимым: OTA завершается на `end`, затем host нужно
перезагрузить перед дальнейшим использованием Wi-Fi.

## Доступность

Доступна в ESP-IDF сборках с ESP-Hosted component. Документ соответствует LexxOS 0.0.7.
