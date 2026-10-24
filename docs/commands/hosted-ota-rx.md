# `hosted-ota-rx`

Обновляет ESP32-C6 ESP-Hosted slave firmware напрямую из UART/YMODEM потока.

Команда полезна, когда SD/FAT ещё не готовы или нужно обновить C6 без промежуточного файла в VFS. Она принимает
`network_adapter.bin` через raw console, проверяет ESP image magic `0xE9` и потоково вызывает
`esp_hosted_slave_ota_write()`.

## Синтаксис

```text
hosted-ota-rx [--dry-run] [--no-activate] [--reboot-host]
```

## Параметры

- `--dry-run` - принять файл и проверить image magic без записи flash C6.
- `--no-activate` - пропустить `esp_hosted_slave_ota_activate()`, даже если текущая C6-прошивка его поддерживает.
- `--reboot-host` - перезагрузить ESP32-P4 после успешной OTA.

## Host-side пример

```bash
PYTHONPATH=tools/lexxcp build/jc-esp32p4-m3-dev-idf602/generated/tools/espidf-python-arm64 - <<'PY'
from pathlib import Path
from lexxcp import ymodem
from lexxcp.serial_shell import LexxSerialClient

port = "/dev/cu.usbserial-XXXX"
image = Path("build/esp-hosted-c6-slave/network_adapter.bin")

client = LexxSerialClient(port, 115200, timeout=0.05)
try:
    client.serial.write(b"\r")
    client.wait_for_prompt(timeout_s=10)
    client.serial.reset_input_buffer()
    client.serial.write(b"hosted-ota-rx --reboot-host\r")
    ymodem.send_file(client.serial, image, image.name)
finally:
    client.close()
PY
```

После OTA и перезагрузки:

```text
hosted-info
wifi-status
```

## Доступность

Доступна в ESP-IDF сборках с ESP-Hosted component. Документ соответствует LexxOS 0.0.7.
