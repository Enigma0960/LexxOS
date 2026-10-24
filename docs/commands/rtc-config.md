# `rtc-config`

save RTC settings for boot restore.

## Синтаксис

```text
rtc-config show
rtc-config set [--enabled yes|no] [--auto-sync yes|no] [--prefer-default yes|no] [--server <host>] [--timeout <ms>] [--interval <ms>] [--retry <ms>] [--startup-delay <ms>] [--tz <offset>] [--source auto|software|picocalc|ds3231|pcf8563] [--bus <name>] [--address <addr>|-]
rtc-config clear
```

## Параметры

- `--enabled yes|no` - включает или выключает software RTC backend.
- `--auto-sync yes|no` - включает автоматическую NTP-синхронизацию software RTC.
- `--prefer-default yes|no` - делает software RTC предпочтительным источником для `--source auto`.
- `--server <host>` / `-n <host>` - NTP сервер.
- `--timeout <ms>` / `-t <ms>` - общий таймаут DNS + NTP, максимум `60000`.
- `--interval <ms>` - интервал успешной автоматической NTP-синхронизации.
- `--retry <ms>` - интервал повтора после неудачной автоматической NTP-синхронизации.
- `--startup-delay <ms>` - задержка software RTC task после старта.
- `--tz <offset>` / `-z <offset>` - фиксированное смещение перед записью в RTC: `UTC`, `Z`, `+HH`, `+HH:MM`,
  `+HHMM`, `-HH`, `-HH:MM`, `-HHMM`.
- `--source ...` / `-s ...` - backend, который `rtc-sync` будет использовать по умолчанию.
- `--bus <name>` / `-b <name>` - I2C bus для внешнего RTC.
- `--address <addr>|-` / `-a ...` - 7-битный адрес внешнего RTC или `-`, чтобы использовать адрес backend-а.

## Пример

```text
lexx:/> rtc-config set --tz +07:00 --server pool.ntp.org --timeout 12000 --source software --auto-sync yes
rtc-config: saved to /mnt/sd/.lexx/rtc.conf

lexx:/> rtc-config show
path: /mnt/sd/.lexx/rtc.conf
configured: yes
runtime-override: yes
enabled: yes
prefer-default: yes
auto-sync: yes
server: pool.ntp.org
timeout-ms: 12000
interval-ms: 3600000
retry-ms: 60000
startup-delay-ms: 3000
tz: +07:00
source: software
bus: debug
address: -
```

После следующего boot LexxOS применит этот файл после mount `/mnt/sd`, если профиль платы включает post-mount restore
для software RTC. На `jc_esp32p4_m3_dev` это происходит перед восстановлением touch calibration и Wi-Fi autoconnect.

`rtc-sync` тоже использует этот файл: сохраненные значения становятся defaults, а явно переданные параметры после
успешной синхронизации записываются обратно в `/mnt/sd/.lexx/rtc.conf`.

## Справка shell

```text
rtc-config - save RTC settings for boot restore

Stores RTC/NTP settings on the SD card and applies them to software RTC auto-sync.

Usage:
  rtc-config show | rtc-config set [--enabled yes|no] [--auto-sync yes|no] [--prefer-default yes|no] [--server <host>] [--timeout <ms>] [--interval <ms>] [--retry <ms>] [--startup-delay <ms>] [--tz <offset>] [--source auto|software|picocalc|ds3231|pcf8563] [--bus <name>] [--address <addr>|-] | rtc-config clear

Arguments:
  --enabled <yes|no> - enable software RTC runtime backend
  --auto-sync <yes|no> - enable automatic NTP sync
  --prefer-default <yes|no> - prefer software RTC for --source auto
  -n, --server <host> - NTP server host name
  -t, --timeout <ms> - overall DNS and NTP timeout
  --interval <ms> - successful NTP sync interval
  --retry <ms> - failed NTP sync retry interval
  --startup-delay <ms> - software RTC task startup delay
  -z, --tz <+HH[:MM]|-HH[:MM]|UTC> - timezone offset applied before writing RTC
  -s, --source <auto|software|picocalc|ds3231|pcf8563> - default rtc-sync backend source
  -b, --bus <name> - default I2C bus for external RTC
  -a, --address <addr|-> - default RTC I2C address or - to use backend default
  action required - configuration action

Runtime:
  launch: foreground-task
  stack: 4096 bytes
  priority: 1
  memory: internal-only
  interface: console
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
