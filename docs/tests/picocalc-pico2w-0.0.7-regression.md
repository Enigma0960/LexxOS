# PicoCalc Pico 2 W 0.0.7 regression checklist

Имена тестовых файлов и идентификаторы портов в примерах условные; результаты измерений сохранены.

Дата serial smoke: 2026-07-08.
Устройство: ClockworkPi PicoCalc with Raspberry Pi Pico 2 W.
Serial: `/dev/tty.usbserial-XXXX`, 115200 baud.
Прошивка: актуальная `0.0.7` перед релизом.

## Автоматический serial smoke

| Блок                    | Команда                                                   | Результат |
|-------------------------|-----------------------------------------------------------|-----------|
| Версия                  | `version`                                                 | PASS: `version: 0.0.7`, `platform: pico`, `board: picocalc_pico2w` |
| Плата                   | `board`                                                   | PASS: ClockworkPi PicoCalc with Raspberry Pi Pico 2 W, RP2350 |
| Память                  | `mem`                                                     | PASS |
| Tasks                   | `tasks`                                                   | PASS: табличный вывод, FreeRTOS tasks видны |
| Devices                 | `devices`                                                 | PASS |
| Storage                 | `storage`                                                 | PASS: SD ready |
| Block devices           | `block-devs`                                              | PASS |
| VFS/FAT                 | `fs-info`                                                 | PASS: `/mnt/sd` mounted |
| VFS cwd                 | `pwd`                                                     | PASS: `/` |
| SD listing              | `ls /mnt/sd`                                              | PASS |
| LCD console             | `console-info`                                            | PASS: `ready: yes`, `text mode: yes`, `53x37` |
| Backlight               | `backlight`                                               | PASS: service-backed levels read |
| Backlight save/restore  | `backlight save all`, `backlight restore all`             | PASS: `/mnt/sd/.lexx/backlight.conf` |
| Display power           | `display-power`, `display-power off`, `display-power on`  | PASS: off/on path restores LCD and keyboard backlight |
| Display timeout         | `display-power timeout 60 sleep`, restore `300 sleep`     | PASS: config saved and restored |
| Southbridge             | `picocalc-sb`, `picocalc-sb --poll 1`                     | PASS: cached snapshot, `keyboard-ready: yes` |
| RTC status              | `rtc-status`                                              | PASS: `source: picocalc-southbridge`, JCS firmware visible |
| RTC read                | `rtc-get`, `rtc-get --source picocalc`                    | PASS |
| NTP/RTC                 | `rtc-sync --tz +07:00`                                    | PASS: `rtc sync: ok` |
| PSRAM info              | `psram-info`                                              | PASS: JEDEC ID `00 00 00 0D 5D 53 31 76` |
| PSRAM data              | `psram-test`                                              | PASS: PIO backend, restored data |
| Beep                    | `beep --frequency 880 --duration 80`                      | PASS: left/right pins printed |
| Audio contract          | `audio`                                                   | PASS: `picocalc-audio`, tone/mix ready |
| Supervisor foreground   | `run board`                                               | PASS |
| Supervisor jobs         | `jobs`, `jobs clear`                                      | PASS |
| Wi-Fi scan              | `wifi-scan`                                               | PASS |
| Wi-Fi config/status     | `wifi-config show`, `wifi-status`                         | PASS: saved config loaded, link up after boot/autoconnect delay |
| Network client          | `ping --count 1 --timeout 10000 8.8.8.8`                  | PASS: `1 packets transmitted, 1 received` |
| NTP network path        | `rtc-sync --tz +07:00`                                    | PASS via `pool.ntp.org` |
| External program demo   | `lexx-run /mnt/sd/ext/hello.lexx alpha beta`              | PASS: program loads, prints `argc/argv`, then waits for keyboard by design |
| External program direct | `/mnt/sd/ext/hello.lexx direct`                           | PASS: direct launch loads and waits for keyboard by design |
| External program cat    | `lexx-run /mnt/sd/ext/cat.lexx /mnt/sd/test.txt`          | PASS: file contents printed |
| Soft reboot             | `reboot`                                                  | PASS: SD automount, backlight restore, prompt returned |
| Post-reboot checks      | `version`, `wifi-status`, `rtc-get`, `console-info`       | PASS |
| Poweroff path           | `poweroff`                                                | PASS: `sync ok`, `/mnt/sd` unmounted, device powered off; boot sequence returned after manual power-on |

## Notes

- `ping --count 1 --timeout 10000 pool.ntp.org` resolved DNS but the single ICMP packet timed out once; `8.8.8.8`
  answered, and `rtc-sync --tz +07:00` completed through `pool.ntp.org`, so Wi-Fi, DNS and UDP/NTP path are confirmed.
- `hello.lexx` is an interactive demo. Timeout in the UART harness is expected after it prints `demo: waiting for keyboard
  input...`; `Ctrl+C` returns the shell.
- `poweroff` no longer returned the older `southbridge request failed` message on this Pico 2 W run. Sync/unmount
  completed, the device powered off, and boot output returned after manual power-on.
- После этого smoke добавлены source fixes для delayed `wifi-autoconnect`/`FreeRTOS task start` output поверх активной
  строки ввода shell, быстрого UART paste и completion длинных MIDI LFN. Host regression tests пройдены; на Pico 2 W UX
  нужно подтвердить при следующей прошивке.
- 2026-07-11 на COM110 отдельно проверены PicoCalc Pico 2 W Game Boy/SD сценарии в 2-core SMP и контрольной 1-core
  сборке: SD/FAT write/sync smoke прошёл, `gbemu` принимал Start+Select и возвращал shell. Одиночный
  `[picocalc-keyboard] communication failure: stage=update-keyboard-fifo-read` появлялся в обоих режимах с `ready=yes`,
  `consecutive=1`, `snapshot-ok=yes`; это зафиксировано как known non-blocking warning, а не SMP-регрессия.
- 2026-07-11 после возврата `wifi-autoconnect` в `perf-picocalc-pico2w-gb` подтверждено, что 266 MHz clock ломал CYW43
  path в этой SMP/Game Boy сборке: `wifi-connect` долго оставался в `join`, а `wifi-scan` таймаутился. На 250 MHz
  autoconnect читает `/mnt/sd/.lexx/wifi.conf`, запускает `wifi-connect`, получает `link: up`, `wifi-scan` видит сети,
  `ping --count 1 --timeout 10000 8.8.8.8` получает `1 received`.
