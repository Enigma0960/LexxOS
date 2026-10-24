# PicoCalc Pico W 0.0.7 regression checklist

Имена тестовых файлов и идентификаторы портов в примерах условные; результаты измерений сохранены.

Дата последнего полного serial smoke: 2026-07-02.
Дата последнего partial serial retest: 2026-07-08.
Дата актуализации checklist: 2026-07-08.
Устройство: ClockworkPi PicoCalc with Raspberry Pi Pico W.
Serial: `/dev/tty.usbserial-XXXX`, 115200 baud.
Прошивка полного smoke: `version: 0.0.7`, `build: Jul 1 2026 21:29:20`, `flavor: standard`.
Прошивка partial retest: `version: 0.0.7`, `build: Jul 8 2026 18:41:11`, `flavor: standard`.

Основной smoke не запускает `poweroff`, `sd-stress` и длинные keyboard live-тесты. Дополнительный soft reboot smoke
запущен отдельно, потому что serial port остаётся доступным после перезагрузки. Клавиатурный прогон считается
подтверждённым вручную перед этим smoke-тестом.

Обновление 2026-07-08: build-only проверка нового RP2040 tight-RAM профиля закрыла link-time SRAM overflow
(`.bss will not fit in region RAM`, overlap с `.stack1_dummy`). Первый hardware retest поймал `display-test` panic из-за
прежней 80 KiB benchmark quota; source fix убрал quota из descriptor и перевёл benchmark buffers на lazy heap allocation.
Повторный hardware retest подтвердил `display-test` и display benchmark path, но выявил нестабильный PSRAM data path при
16 MHz из старого CMake cache. После retest CMake cap для `picocalc_pico_w` снижает PSRAM baud до 8 MHz; новый UF2 нужно
прошить и повторить PSRAM checks. После отчёта о перекрытии prompt сообщениями `wifi-autoconnect`/`FreeRTOS task start`
также добавлен source fix для перерисовки активной строки ввода. Следующий UART retest выявил, что быстрый paste
теряет байты после 32-байтного FIFO, а path completion режет длинные MIDI LFN до 32 символов; эти source fixes тоже
нужно проверить на том же новом UF2.

```text
LEXXOS_PICO_FREERTOS_HEAP_KB=88
LEXXOS_PICOCALC_PSRAM_SPI_BAUDRATE=8000000
LEXXOS_FEATURE_GAMEBOY=OFF
LEXXOS_FEATURE_MIDI=OFF
LEXXOS_FEATURE_GPIO_ANALYZER=OFF
```

Map для tight-RAM build до lazy workspace fix: `.data=0x106a8`, `.bss=0x2d3e0`, FreeRTOS `ucHeap=0x16000`,
`.heap=0x800`, `.stack1_dummy=0x20040000`. После hardware retest `display-test --bench` дополнительно переведён на
ленивые heap buffers: обычный `display-test` больше не резервирует benchmark workspace, а дефолтный `--rows` равен 16.

## Автоматический serial smoke

| Блок                    | Команда                              | Ожидание                                                                            | Результат                                     |
|-------------------------|--------------------------------------|-------------------------------------------------------------------------------------|-----------------------------------------------|
| Версия                  | `version`                            | `version: 0.0.7`, `platform: pico`, `board: picocalc_pico_w`                        | PASS                                          |
| Плата                   | `board`                              | PicoCalc Pico W descriptor, PSRAM/audio/wireless pins                               | PASS                                          |
| Память                  | `mem`                                | heap статистика печатается                                                          | PASS                                          |
| Tasks                   | `tasks`                              | табличный вывод, console/input/system/console-mirror tasks видны                    | PASS                                          |
| Devices                 | `devices`                            | табличный вывод: `sd0`, `ili9488-spi`, `picocalc-keyboard`, `picocalc-southbridge`  | PASS                                          |
| Storage                 | `storage`                            | SD ready, SPI backend, block size/count, capacity                                   | PASS                                          |
| Block devices           | `block-devs`                         | табличный вывод, `sd0 ready free`                                                   | PASS                                          |
| VFS/FAT                 | `fs-info`                            | табличный вывод, `/mnt/sd` mounted as `fatfs ready rw`                              | PASS                                          |
| VFS cwd                 | `pwd`                                | `/`                                                                                 | PASS                                          |
| SD listing              | `ls /mnt/sd`                         | табличный вывод, SD root directory is readable, no stack overflow                   | PASS                                          |
| LCD console             | `console-info`                       | text mode, scrollback/status bar visible                                            | PASS                                          |
| Backlight               | `backlight`                          | source `picocalc-southbridge`, lcd/kbd values printed                               | PASS, with transient keyboard comm warning    |
| Display power           | `display-power`                      | `ili9488-spi mode=on`                                                               | PASS                                          |
| Southbridge             | `picocalc-sb`                        | cached service snapshot, `keyboard-ready: yes`                                      | WARN: cached snapshot can be partial          |
| Southbridge FIFO safety | `picocalc-sb --poll 1`               | direct FIFO polling skipped while keyboard driver owns input                        | PASS                                          |
| RTC status              | `rtc-status`                         | source `picocalc-southbridge`, JCS firmware visible                                 | PASS                                          |
| RTC read                | `rtc-get`                            | decoded date/time                                                                   | PASS after `rtc-set --source picocalc` retake |
| PSRAM info              | `psram-info`                         | JEDEC ID printed                                                                    | PASS                                          |
| PSRAM data              | `psram-test`                         | `ok restored=yes`                                                                   | PASS                                          |
| Beep                    | `beep --frequency 880 --duration 80` | left/right pins printed, short beep expected                                        | PASS, audible on hardware                     |
| Audio contract          | `audio`                              | `picocalc-audio`, tone/mix/channel capabilities                                     | PASS                                          |
| Supervisor foreground   | `run board`                          | board descriptor printed through supervised task                                    | PASS, harness timeout after output; exit 0    |
| Supervisor jobs         | `jobs`                               | finished `board`, stack/memory fields visible                                       | PASS                                          |
| Supervisor cleanup      | `jobs clear`                         | finished process cleared                                                            | PASS                                          |
| Wi-Fi scan              | `wifi-scan`                          | scan completes and prints merged networks                                           | PASS                                          |
| Wi-Fi config            | `wifi-config show`                   | config path/status printed without password                                         | PASS                                          |
| Wi-Fi connect/status    | `wifi-status`                        | after saved auto-connect or manual connect, link/IP/DNS are visible                 | PASS                                          |
| Network client          | `ping --count 1 pool.ntp.org`        | DNS resolves and one ICMP request completes                                         | PASS                                          |
| NTP/RTC                 | `rtc-sync --tz +07:00`               | NTP query succeeds and selected RTC backend is updated                              | RETEST: source fixed after flashed smoke      |

## Build-only SRAM retest 2026-07-08

| Блок             | Команда                                                        | Результат |
|------------------|----------------------------------------------------------------|-----------|
| Pico W build     | `cmake --build build/configurator/picocalc-pico-w --target lexxos` | PASS      |
| Pico W size/map  | `arm-none-eabi-size build/configurator/picocalc-pico-w/src/lexxos.elf` + map inspection | PASS: no RAM overlap, FreeRTOS heap 88 KiB |
| Pico 2 W control | `cmake --preset debug-picocalc-pico2w`; `cmake --build --preset lexxos-picocalc-pico2w` | PASS      |
| Host control     | `cmake --build build/host-full --target lexxos`                | PASS      |
| Lazy workspace fix | `cmake --build build/configurator/picocalc-pico-w --target lexxos` after source fix | PASS: `text=835000`, `bss=187552`, UF2 rebuilt |
| PSRAM baud cap    | `cmake --build build/configurator/picocalc-pico-w --target lexxos` after CMake cap | PASS: CMake reduced old cache `16000000 -> 8000000`, UF2 rebuilt |
| Console prompt guard | `lexxos_services_contract_tests`, host CTest, Pico W/Pico 2 W firmware builds | PASS: chunked async output redraws saved shell input; Pico W `text=836160`, `bss=187576` |
| UART paste and long LFN completion | `lexxos_contract_tests`, `lexxos_services_contract_tests`, host CTest, Pico W/Pico 2 W firmware builds | PASS: batch input drain, VFS name limit 64; Pico W `text=836400`, `bss=188056` |

## Hardware partial retest 2026-07-08

Проверенная прошивка: `build: Jul 8 2026 18:41:11`, после tight-RAM split, но до lazy display benchmark workspace fix.

| Блок         | Команда                              | Результат |
|--------------|--------------------------------------|-----------|
| Версия       | `version`                            | PASS: `board: picocalc_pico_w`, `target: PICO cortex-m0plus` |
| Плата        | `board`                              | PASS: RP2040/PICO, SD SPI 50 MHz, display SPI 50 MHz, PSRAM 16 MHz, audio, CYW43 |
| Память       | `mem`                                | PASS: `free heap: 47552 bytes`, `minimum ever free heap: 37056 bytes` |
| Runtime      | `top --count 1 --no-clear`           | PASS: internal heap `free=43328B`, task stacks `used=10092B`, arena `reserved=0B` |
| Tasks        | `tasks`                              | PASS |
| Devices      | `devices`                            | PASS: display/input busy by console, audio and southbridge service ready |
| Storage      | `storage`, `block-devs`, `fs-info`   | PASS: `/mnt/sd` mounted `fatfs ready rw`, SD capacity около 30436 MiB |
| VFS/FAT      | `pwd`, `ls /mnt/sd`                  | PASS |
| SD speed     | `sd-baud`, `sd-test`, `fs-smoke`, `sd-bench` | PASS: raw read 259 KiB/s, FAT write 166 KiB/s, FAT read/verify 500 KiB/s |
| Console      | `console-info`                       | PASS: battery 100%, Wi-Fi connected, status time visible |
| Display info | `display-info`                       | PASS |
| Display draw | `display-test`                       | FAIL: `panic: FreeRTOS malloc failed` |

Вывод: падение было не в LCD draw path, а в descriptor-level reservation для benchmark workspace. На RP2040 после boot
оставалось около 47 KiB heap, а команда заранее запрашивала около 80 KiB даже для обычного pattern mode. Source fix после
этого retest убрал descriptor quota и выделяет benchmark buffers лениво; нужен повторный hardware smoke после перепрошивки
нового UF2. Короткая UART-проба `version` после panic вернула `NO_RESPONSE`, поэтому текущая прошивка считается
остановленной до power cycle/reflash.

Hardware статус: RETEST. После прошивки нового UF2 повторить `mem`, `top`, SD/FAT, Wi-Fi, `display-test`,
`display-test --bench` и smoke-команды ниже.

## Hardware retest after lazy workspace fix 2026-07-08

Проверенная прошивка: `build: Jul 8 2026 18:41:11`, после lazy display benchmark workspace fix, но до PSRAM 8 MHz cap.
Board output всё ещё показывал PSRAM baud `16000000`.

| Блок              | Команда                              | Результат |
|-------------------|--------------------------------------|-----------|
| Версия/плата      | `version`, `board`                   | PASS: `picocalc_pico_w`, RP2040, SD 50 MHz, display 50 MHz, PSRAM 16 MHz |
| Память            | `mem`, `top --count 1 --no-clear`    | PASS: heap `47552` bytes, lowest before benchmark `37056`, task stacks sane |
| Display pattern   | `display-test`                       | PASS: `drawn: 320x320`, heap restored to `47552` bytes |
| Display benchmark | `display-test --bench --frames 1 --transport double --size full --rows 16` | PASS: 256 ms, `ok=yes`, min heap `21816` |
| Display matrix    | `display-test --bench --frames 1 --transport all --size gb1 --rows 16` | PASS: row/stream/double/cached/psram transports all `ok=yes` |
| Heap guard        | `display-test --bench --frames 1 --transport double --size full --rows 64` | PASS: graceful `not enough heap`, requested `81920`, no panic |
| SD/FAT            | `sd-test`, `fs-smoke`, `sd-bench`    | PASS: raw 242 KiB/s, FAT write 193 KiB/s, FAT read/verify 492 KiB/s |
| Southbridge       | `backlight`, `picocalc-sb`, `picocalc-sb --poll 1` | PASS with one transient `update-keyboard-fifo-read`; full snapshot stayed ok |
| RTC/Wi-Fi         | `wifi-config show`, `wifi-scan`, `wifi-status`, `ping`, `rtc-sync --tz +07:00` | PASS: link up, ping `8.8.8.8`, `rtc sync: ok` |
| Soft reboot       | `reboot`, then `version`, `wifi-status`, `rtc-get`, `console-info`, `mem` | PASS: SD automount, backlight restore, shell returned, Wi-Fi up after delay |
| PSRAM probe       | `psram-info`                         | WARN: JEDEC bytes `FF FF FC 0D 5D 53 31 76`, PIO backend at 16 MHz |
| PSRAM data        | `psram-test`, plus addresses `0x00200000` and `0x00001000` | FAIL: first byte readback `D2` instead of pattern, restore mismatch, `restored=no` |

Вывод: display-test stack/heap blocker закрыт. Оставшийся блокер первого Pico W — PSRAM data path при 16 MHz из старого
CMake cache. После этого retest `CMakeLists.txt` получил release cap: для `LEXXOS_BOARD=picocalc_pico_w` старые cache
values выше 8 MHz принудительно снижаются до `8000000`. Нужно прошить новый UF2 и повторить минимум `board`,
`psram-info`, `psram-test`, `display-test --bench --frames 1 --transport all --size gb1 --rows 16`.

## Source fix after prompt redraw report 2026-07-08

Симптом на UART: delayed `wifi-autoconnect: connecting ...` и `FreeRTOS task start: ...` печатались поверх строки
`lexx:/>`, переносили курсор и оставляли уже набранный текст визуально потерянным до следующей принудительной
перерисовки, например Tab completion.

Source fix:

- `console_output` получил interactive line guard: внешний вывод очищает текущую строку, печатает сообщение кусками и
  после `\n` вызывает redraw;
- `Shell` регистрирует guard только когда prompt реально активен, снимает его на Enter/запуске программы/фокусе
  foreground app и сохраняет текущий `_lineBuffer`;
- FreeRTOS scheduler console writer в runtime теперь идёт через `console_output`, поэтому `FreeRTOS task start` проходит
  тем же путём, что Wi-Fi/system-event сообщения.

Локальный статус: PASS в host regression test. Hardware статус: RETEST. После прошивки нового UF2 проверить:

```text
# оставить сохранённый Wi-Fi config, перезагрузить плату, сразу набрать несколько символов на prompt
# дождаться wifi-autoconnect / FreeRTOS task start
# убедиться, что сообщение появилось строкой выше, а prompt сохранил введённый текст
```

## Source fix after UART paste and LFN completion report 2026-07-08

Симптомы на текущей прошивке:

- быстрый paste в UART вставлял только первые 32 байта, например `echo 0123456789ABCDEFGHIJKLMNOPQ`;
- Tab completion для `/mnt/sd/midi/ExampleMidiFile_T...` перерисовывал строку как
  `mplayer /mnt/sd/midi/ExampleMidiFile_Theme_One_Long1. `, теряя `.mid`;
- более длинное имя `ExampleMidiFile_Finale_With_A_Long_Title.mid` невозможно было получить через completion.

Source fix:

- console task теперь использует `console_input::readAvailable()` и сначала вычитывает доступный input batch в RAM, чтобы
  не переполнять 32-байтный UART FIFO во время echo;
- `VfsRuntimeLimits::maxNameLength` поднят с 32 до 64, что совпадает с текущим FatFs `FF_LFN_BUF=64`;
- host tests покрывают paste-sized platform input и completion длинного файла
  `ExampleMidiFile_Finale_With_A_Long_Title.mid`.

Локальный статус: PASS. Hardware статус: RETEST. После прошивки нового UF2 проверить:

```text
echo 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz
mplayer /mnt/sd/midi/ExampleMidiFile_T<Tab>
mplayer /mnt/sd/midi/ExampleMidiFile_F<Tab>
```

## Замечания по прогону 2026-07-02

- `ls /mnt/sd`, `tasks`, `devices`, `block-devs`, `fs-info` и `jobs` прошли через `PrettyTable` без panic и stack overflow.
- `beep` дал несколько слышимых звуковых сигналов на реальном PicoCalc; `audio` подтвердил `picocalc-audio` tone/mix
  contract.
- Boot auto-connect поднял Wi-Fi: `wifi-status` показал `link: up`, IP, gateway и DNS; `ping --count 1 pool.ntp.org`
  получил `1 received`.
- Первый `rtc-get` до установки времени вернул `failed to read or decode RTC date/time`. Повторный
  `rtc-set --source picocalc ...` и `rtc-get --source picocalc` подтвердили чтение/запись PicoCalc RTC backend.
- `rtc-sync --timeout 30000 --tz +07:00` и `rtc-sync --server time.google.com --timeout 30000 --tz +07:00` доходят до
  NTP request, но завершаются `rtc-sync: failed to write RTC date/time`. После этого `rtc-get` читает актуальное время,
  поэтому нужно отдельно разобрать JCS RTC write acknowledgement/echo path.
- После этого прогона source fix добавил readback fallback для PicoCalc JCS RTC write: если strict time-register echo
  отличается на тикающую секунду, но readback подтверждает установленное время, команда должна вернуть success. Нужно
  повторить `rtc-sync --tz +07:00` после прошивки нового UF2.
- `backlight` и обычный `picocalc-sb` иногда печатают `[picocalc-keyboard] communication failure:
  stage=update-keyboard-fifo-read`, после чего cached snapshot может содержать только keyboard-state и read-failed поля.
  Это оформлено как known non-blocking warning, если ошибка одиночная, ввод не теряется, а `ready=yes`,
  `consecutive=1`, `snapshot-ok=yes`. `picocalc-sb --poll 1` в том же прогоне сделал full refresh и сохранил FIFO
  safety (`direct polling skipped`).

## Soft reboot smoke

| Блок                 | Команда                       | Результат                                                               |
|----------------------|-------------------------------|-------------------------------------------------------------------------|
| Reboot               | `reboot`                      | PASS: serial port stayed available                                      |
| Boot sequence        | boot output                   | PASS: SD automount, saved backlight restore, Wi-Fi autoconnect, banner  |
| Shell after reboot   | prompt                        | PASS: `LexxOS shell` and `lexx:/>` shown                                |
| Version after reboot | `version`                     | PASS: output complete; harness split output across next command capture |
| Wi-Fi after reboot   | `wifi-status`                 | PASS: `link: up`, IP/gateway/DNS visible                                |
| RTC after reboot     | `rtc-get --source picocalc`   | PASS: decoded date/time visible                                         |
| LCD status after boot | delayed `console-info`       | PASS: status bar, battery, Wi-Fi and time visible                       |
| Southbridge after reboot | `picocalc-sb --poll 1`     | PASS: full snapshot, `snapshot-last-refresh-ok: yes`, FIFO skipped      |

## Ручные или отдельные проверки

| Блок     | Проверка                                                         | Статус                                                                             |
|----------|------------------------------------------------------------------|------------------------------------------------------------------------------------|
| Keyboard | `input-test`, `keyboard-live`, editor input, arrows/modifiers    | PASS, подтверждено вручную                                                         |
| RTC write ack | `rtc-sync` пишет PicoCalc JCS RTC и возвращает `rtc sync: ok` | RETEST after source fix                                                            |
| Southbridge snapshot | `picocalc-sb` без transient read-failed полей          | TODO                                                                               |
| Async console redraw | typed prompt survives delayed Wi-Fi/task messages      | RETEST after source fix                                                            |
| UART paste and long LFN completion | fast paste and long MIDI path completion | RETEST after source fix                                                            |
| Poweroff | `poweroff` через JCS southbridge                                 | Known limitation: может вернуть `southbridge request failed`; не запускать в smoke |
| MIDI     | `mplayer`                                                        | Disabled by default in standard build via `LEXXOS_FEATURE_MIDI=OFF`                |
| PrettyTable stack | `ls`, `mount`, `tasks`, `jobs`, `devices`, `block-devs`    | PASS                                                                               |

## Команды для повторения

```text
version
board
mem
tasks
devices
storage
block-devs
fs-info
pwd
ls /mnt/sd
console-info
backlight
display-power
picocalc-sb
picocalc-sb --poll 1
rtc-status
rtc-get
psram-info
psram-test
beep --frequency 880 --duration 80
audio
run board
jobs
jobs clear
wifi-scan
wifi-config show
wifi-status
ping --count 1 pool.ntp.org
rtc-sync --tz +07:00
```
