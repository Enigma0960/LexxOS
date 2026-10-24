# M5Stack Cardputer-Adv 0.0.7 regression checklist

Имена тестовых файлов и идентификаторы портов в примерах условные; результаты измерений сохранены.

Дата serial smoke: 2026-07-13.
Устройство: M5Stack Cardputer-Adv / ESP32-S3FN8 Stamp-S3A.
Serial: `/dev/tty.usbmodemXXXX`, 115200 baud shell.
Прошивка: `user-espidf-m5stack_cardputer`, board profile `m5stack_cardputer_adv`.

## Сборка и прошивка

```bash
cmake --preset user-espidf-m5stack_cardputer
cmake --build --preset user-espidf-m5stack_cardputer-build
ESPPORT=/dev/tty.usbmodemXXXX ESPBAUD=921600 cmake --build build/configurator/cardputer --target app-flash
```

Финальный build после Game Boy mapped-flash / scheduler slot reuse / PCM release / launcher audio patch:

- build: PASS;
- compile definitions: PASS, ESP-IDF component command contains `LEXXOS_GAMEBOY_ROM_CACHE_BANKS=3`,
  `LEXXOS_GAMEBOY_ROM_CACHE_PAGE_BYTES=8192` and `LEXXOS_GAMEBOY_DYNAMIC_ROM_CACHE=1`;
- image size: PASS, `LexxOS.bin` `0xdcad0` bytes, app partition free `0x213530` bytes (71%);
- flash: PASS through `/dev/tty.usbmodemXXXX`, `ESPBAUD=921600`;
- build timestamp from `version`: `Jul 13 2026 09:58:01`.

## Serial smoke

| Блок | Команда | Результат |
|------|---------|-----------|
| Версия | `version` | PASS: `platform: espidf`, `board: m5stack_cardputer_adv`, `milestone: fat-minimal-v1`; local `version:` field empty in this build |
| Devices | `devices` | PASS: `sd0`, `m5stack-lcd`, `cardputer-keyboard`, `cardputer-gamepad`, `cardputer-bmi270`, `cardputer-ir`, `cardputer-adv-audio`, `cardputer-adv-mic` all `ready` |
| LCD | `display-info`, `display-test` | PASS in previous same-day pass: `240x135`, `rgb565`, full-screen draw path works |
| SD/FAT | `ls /mnt/sd/gb`, `cat /mnt/sd/apps/gbemu/settings.ini`, `gb-info /mnt/sd/gb/test-rom-2.gb` | PASS: FAT listing and `cat` work without stack overflow; ROM header `title: ZELDA`, MBC1+RAM+BATTERY, checksum ok |
| Power | `power` | PASS: `source=cardputer-adc-gpio10`, `4126mV`, `97%`, `charging=unavailable` |
| Status bar | `console-info` | PASS: `status battery: 99% charging=no`, status bar updated |
| Keyboard idle | `keyboard-live -s 3 -i 50` | PASS: `events=0`, `changes=0`, no idle drift while USB Serial/JTAG is connected |
| Gamepad idle | `gamepad-test`, `gamepad-live -s 3 -i 50` | PASS: all buttons 0; `changes=0` |
| Gamepad mapping | board profile | PASS by code review: `e/z/a/d` => up/down/left/right, `[`/`]` => A/B, `;` => X, grave key => Y, Enter => Start, Backspace => Select |
| BMI270 | `imu -n 3 -i 50` | PASS: `chip=0x24`, `status=0xD0`, nonzero accel/gyro, temp around `34.6C` |
| IR info | `ir info` | PASS: `cardputer-ir`, `pin: 44`, `carrier-hz: 38000`, Samsung codes printed for power/on/off |
| IR transmit | `ir samsung-power -r 1` | PASS: serial reports `ir: sent`; receiver/TV reaction is a manual line-of-sight check |
| Audio info | `audio` | PASS in previous same-day pass: `cardputer-adv-audio`, `tone: yes`, `mix: no`, `pcm: yes` |
| Audio PCM | `mplay --test` | PASS in previous same-day pass: `441000` PCM frames, no underruns/full waits, about 10-11 seconds |
| Microphone sample | `mic sample` | PASS after `gbemu --audio`: `frames=512`, `sample-rate=44100`, `peak=32768`, `average=14501` |
| Microphone loopback | `mic loopback -s 1 -g 2` | PASS after `gbemu`: `frames=43520`, `chunks=85`, `peak=32768` |
| Game Boy headless | `gbemu --profile --frames 60 --display none /mnt/sd/gb/test-rom-2.gb` | PASS: old settings still had `rom_storage = file-cache`, but playable default selected `rom-storage=flash`; `rom-cache=8Kx6=48K`, `cache-fill=0`, `wall-ms=808`, `fps=74.25`, `realtime-x=1.24` |
| Game Boy display | `gbemu --frames 60 --display crop /mnt/sd/gb/test-rom-2.gb` | PASS: `display-mode: crop`, `destination: 160x135 @ 40,0`, `cpu-ms=691`, `ppu-ms=74`, `blit-ms=211`, returned to shell |
| Game Boy audio | `gbemu --audio --frames 2 --display none /mnt/sd/gb/test-rom-2.gb` twice | PASS: both runs used `rom-storage=flash`, `audio=1`, `status=ok`; `mem` returned to about `86456 bytes` after PCM output release |
| Game Boy long audio | `gbemu --audio --profile --frames 600 --display none /mnt/sd/gb/test-rom-2.gb` | PASS: `audio=1`, `audio frames queued/written=221517/221517`, `wall-ms=9888`, `fps=60.67`, `realtime-x=1.01`, `status=ok` |
| Game Boy launcher audio defaults | `gbemu-config --show-defaults`, code path | PASS: Cardputer-Adv defaults include `[audio] enabled = true`; legacy settings without `[audio]` parse to the target default, and launcher passes `--audio` when the setting is on |
| Scheduler slots | 14 foreground commands including `cat`, `gbemu`, `mic`, `tasks` | PASS: no `task-create-failed`; `tasks` after the sequence listed only `console-mirror`, `console`, `cardputer-input` and `system-events` |
| Heap after Game Boy | `mem` | PASS: after mapped-flash Game Boy + PCM + mic sequence `68272 bytes`, minimum-ever `47124 bytes`; scheduler task list stayed clean |
| Clear | `clear`, visual check | PASS in previous same-day pass: shared console clear path clears the full LCD text area below status bar |

## Audio / Microphone Notes

Cardputer-Adv PCM regression was compared with `temp/temp/NormalTest.wav`, recorded from PicoCalc before the M5 audio
fixes. The 2026-07-13 Game Boy audio pass also recorded through macOS `ffmpeg` from `Wireless GO RX` to
`/tmp/audio-test.wav` while running `gbemu --audio --profile --frames 600`; the capture had peak `23226`
and low RMS, consistent with a quiet/sparse captured line-level signal or gain chain rather than an acoustic distance
effect. Serial counters confirmed `221517/221517` PCM frames written.

The microphone is exposed separately as `cardputer-adv-mic`, but uses the same ES8311/I2S hardware path. `mic loopback`
is a short audible test: it captures 512-frame chunks and sends them to the primary `AudioDevice`. ES8311 output now
releases I2S/DMA after `drainPcmOutput()`/`stopPcmOutput()`, and the registry lazily reinitializes the codec for the
next audio or microphone request.

## Power Notes

M5Stack Cardputer/Cardputer-Adv аппаратно не предоставляют достоверный charging status или current reading. LexxOS
читает только ADC-напряжение батареи на GPIO10 и оценивает процент. Поэтому:

- `power` должен печатать `charging=unavailable`;
- статус-бар должен показывать процент, но не charging icon;
- при USB-питании процент может быть завышен из-за измеряемого напряжения;
- для зарядки Cardputer-Adv боковой выключатель должен быть в положении `ON`.

## Sensor / IR / Game Boy Notes

Cardputer-Adv BMI270 answers at I2C address `0x69` on the shared system/debug I2C bus (`G8/G9`). LexxOS uploads the
Bosch BMI270 config file during board-device registration; without that upload the chip id is readable, but accel/gyro
data remains zero/invalid.

Cardputer-Adv IR is registered as `cardputer-ir` on GPIO44. `ir samsung-power` sends a 38 kHz Samsung power test frame
with code `0xE0E040BF`; `samsung-on`, `samsung-off` and `samsung-code -c <hex>` are available for manual variants.
Serial smoke verifies transmit completion only.

Game Boy on Cardputer-Adv depends on the ESP-IDF component receiving the ROM cache compile definitions. Without them
the workspace/cache path stayed too large for internal RAM. The current preset uses `8Kx6=48K` dynamic cache and, for
playable runs, mapped flash staging in the ESP-IDF `storage` partition at `0x300000` with 1 MiB capacity. First use
copies and verifies the ROM; repeated runs print `reused mapped flash ROM bytes=524288` and avoid SD/FAT cache misses.
