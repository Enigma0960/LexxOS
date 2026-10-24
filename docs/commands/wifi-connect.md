# wifi-connect

Подключает активную плату к Wi-Fi сети через board-level WiFi backend.

## Синтаксис

```text
wifi-connect [--timeout <ms>] [--auth auto|open|wpa|wpa2|mixed] <ssid> [password]
```

## Параметры

- `--timeout <ms>` / `-t <ms>` — максимальное время одной попытки подключения; по умолчанию `30000`, максимум `120000`.
- `--auth <mode>` / `-a <mode>` — режим аутентификации: `auto`, `open`, `wpa`, `wpa2`, `mixed`; по умолчанию `auto`.
- `<ssid>` — имя Wi-Fi сети, до 32 байт.
- `[password]` — пароль сети. Для `open` можно не указывать.

На Pico/CYW43 в режиме `auto` команда пробует `mixed`, затем `wpa2`, затем `wpa`. Это полезно для бытовых роутеров,
где scan показывает сеть как `wpa2`, но подключение через строгий `wpa2` завершается timeout. На ESP-IDF backend
используется native STA configuration path.

## Примеры

```text
lexx:/> wifi-scan
lexx:/> wifi-connect MyNetwork mysecretpassword
wifi-connect: connecting to MyNetwork auth=auto...
wifi-connect: connected
```

Открытая сеть:

```text
lexx:/> wifi-connect --auth open Guest
```

## Замечания

- Команда не сохраняет пароль; для автоподключения используйте `wifi-config set`.
- Доступность команды определяется `board.wireless.wifi` и platform backend.
- Для ESP32-P4 hosted WiFi команда работает через ESP-IDF `esp_wifi` / `esp_hosted` STA interface.

## История shell

Вызовы этой команды не попадают в историю, включая ошибочные вызовы и подкоманды без пароля.
Старые записи фильтруются при загрузке; `shell-config save` перезаписывает файл истории без них.
Введённый текст по-прежнему может присутствовать в UART/терминальном логе.
