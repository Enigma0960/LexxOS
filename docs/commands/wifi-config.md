# wifi-config

Сохраняет параметры Wi-Fi сети для автоматического подключения платы после загрузки.

Конфигурация хранится на SD-карте:

```text
/mnt/sd/.lexx/wifi.conf
```

## Синтаксис

```text
wifi-config set [--auth auto|open|wpa|wpa2|mixed] [--timeout <ms>] <ssid> [password]
wifi-config show
wifi-config clear
```

## Параметры

- `set` / `save` — сохранить сеть для автоподключения.
- `show` / `status` — показать текущую конфигурацию без вывода пароля.
- `clear` / `remove` / `forget` — удалить сохранённую конфигурацию.
- `--auth <mode>` / `-a <mode>` — режим аутентификации; по умолчанию `auto`.
- `--timeout <ms>` / `-t <ms>` — таймаут подключения; по умолчанию `30000`, максимум `120000`.
- `<ssid>` — имя Wi-Fi сети, до 32 байт.
- `[password]` — пароль сети, до 64 байт. Для WPA/WPA2 нужен пароль не короче 8 символов.

## Примеры

```text
lexx:/> wifi-config set MyNetwork mysecretpassword
wifi-config: saved MyNetwork to /mnt/sd/.lexx/wifi.conf
wifi-config: warning: password is stored as plain text on the SD card
```

После перезагрузки LexxOS автоматически прочитает `/mnt/sd/.lexx/wifi.conf` и запустит `wifi-connect`.
На платах, где SD монтируется асинхронно, автоподключение дополнительно запускается сразу после успешного mount
`/mnt/sd`.

Проверка конфигурации:

```text
lexx:/> wifi-config show
path: /mnt/sd/.lexx/wifi.conf
enabled: yes
ssid: MyNetwork
auth: auto
timeout: 30000 ms
password: set
```

## Замечания

- Пароль хранится на SD-карте в открытом виде.
- Старый файл `/mnt/sd/lexxos/wifi.conf` читается как legacy-конфигурация и при успешном чтении мигрируется в
  `/mnt/sd/.lexx/wifi.conf`.
- Автоподключение запускается после монтирования SD-карты и старта FreeRTOS scheduler, если профиль платы включает
  `board.wireless.autoConnect`.
- Если файла нет или `enabled=0`, автоподключение пропускается без ошибки.

## История shell

Вызовы этой команды не попадают в историю, включая ошибочные вызовы и подкоманды без пароля.
Старые записи фильтруются при загрузке; `shell-config save` перезаписывает файл истории без них.
Введённый текст по-прежнему может присутствовать в UART/терминальном логе.

Исключение из истории не меняет назначение `wifi-config set`: credentials для автоподключения сохраняются
отдельно в `wifi.conf` в открытом виде.
