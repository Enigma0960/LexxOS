# ClockworkPi PicoCalc / Pico, Pico W, Pico 2, Pico 2 W

Публичный набор 0.1.0 включает minimal/full для всех четырёх модулей:

| Модуль | LEXXOS_BOARD | PICO_BOARD |
| --- | --- | --- |
| Pico | `picocalc_pico` | `pico` |
| Pico W | `picocalc_pico_w` | `pico_w` |
| Pico 2 | `picocalc_pico2` | `pico2` |
| Pico 2 W | `picocalc_pico2w` | `pico2_w` |

У non-W модулей нет CYW43; команды Wi-Fi исключены. Публичные presets используют
**JCS southbridge firmware**, а не factory profile. Перед прошивкой сверяйте модуль
и [версию southbridge](../picocalc-jcs-keyboard-firmware.md).
Все варианты прошли сборку/упаковку; hardware smoke финального набора не выполнен.

## Исторические аппаратные проверки

В 0.0.7 Pico W проверялся как автономное устройство: LCD shell/editor, клавиатура,
SD/FAT и Wi-Fi; Pico 2 W прошёл отдельный UART smoke 2026-07-08.
Утверждения о подтверждённой работе оборудования ниже относятся к этим проверкам,
а не к новым образам 0.1.0. Для Pico/Pico 2 без W аппаратные результаты не получены.
Bluetooth отложен, PSRAM проверялась диагностически, audio/MIDI не прошли полную
настройку на устройстве. [Текущие результаты выпуска](../tests/release-0.1.0-validation.md).

## Аппаратная конфигурация

- Основной MCU: Raspberry Pi Pico W, RP2040.
- UART-консоль: UART0, TX GP0, RX GP1, 115200 baud.
- USB CDC console mirror: включён по умолчанию для PicoCalc; USB product string `LexxOS PicoCalc`; shell output идёт в
  UART и USB, input принимается из USB или UART.
- System I2C / southbridge: I2C1, SDA GP6, SCL GP7, 400 kHz.
- SD card: SPI0, MISO GP16, CS GP17, SCK GP18, MOSI GP19, card detect GP22 active-low, штатная частота передачи 50 MHz.
- LCD: ILI9488-compatible 320x320 panel on SPI1, SCK GP10, MOSI GP11, CS GP13, DC GP14, RESET GP15.
- Wireless: CYW43 Wi-Fi/Bluetooth capability объявлен в board profile; Wi-Fi scan, connect/status/ping и boot
  auto-connect подтверждены, а `rtc-sync --tz +07:00` прошёл hardware retest на PicoCalc Pico 2 W.
- PSRAM: CE GP20, SCLK GP21, SI/SIO0 GP2, SO/SIO1 GP3, SIO2 GP4, SIO3 GP5 по
  `clockwork_Mainboard_V2.0_Schematic`; JEDEC ID подтверждён как `00 00 00 0D 5D 53 31 76`.
- Audio beep: PWM left GP26, PWM right GP27 по `clockwork_Mainboard_V2.0_Schematic`; проверка left/right подтверждена.
- Status LED: отключён, потому что Pico W не выводит классический Pico GP25 LED.
- PicoCalc southbridge address: `0x1F`.

## Configure and build

```bash
cmake --preset minimal-picocalc_pico_w
cmake --build --preset minimal-picocalc_pico_w
```

UF2-файл ожидается здесь:

```text
build/minimal-picocalc_pico_w/src/lexxos.uf2
```

Для других модулей замените board id по таблице, для full — префикс `minimal-`
на `full-`. Готовый релизный архив содержит `firmware.uf2`: удерживая BOOTSEL,
подключите модуль по USB и скопируйте UF2 на появившийся загрузочный диск.

## CMake GUI / CLion notes

Для Pico W первого поколения важна следующая пара настроек:

```text
LEXXOS_BOARD=picocalc_pico_w
PICO_BOARD=pico_w
```

Не используйте `picocalc_pico2w` вместе с `PICO_BOARD=pico_w`: такая комбинация собирает RP2040-образ с board descriptor
для RP2350/Pico 2 W. Если GUI-конфигурация продолжает выбирать старый профиль платы, удалите соответствующий build
directory или очистите CMake cache перед повторной конфигурацией.

Для RP2040 (Pico и Pico W) включён ограниченный по памяти профиль:

```text
LEXXOS_PICO_FREERTOS_HEAP_KB=88
LEXXOS_PICOCALC_PSRAM_SPI_BAUDRATE=8000000
LEXXOS_FEATURE_GAMEBOY=OFF
LEXXOS_FEATURE_MIDI=OFF
LEXXOS_FEATURE_GPIO_ANALYZER=OFF
```

Эти ограничения нужны именно для RP2040 SRAM. Ошибка вида `.bss will not fit in region RAM` или overlap с
`.stack1_dummy` означает link-time переполнение статической памяти, а не обычный runtime stack overflow. Тяжёлые фичи
можно включить только для отдельной экспериментальной сборки через `LEXXOS_ALLOW_PICOCALC_PICO_W_HEAVY_FEATURES=ON`;
такой образ не входит в публичный набор 0.1.0. Старый CMake cache с heap больше 88 KiB будет автоматически снижен до
релизного значения.

Старый CMake cache с PSRAM baud выше 8 MHz тоже автоматически снижается для `picocalc_pico_w`. Hardware retest на RP2040
показал неверный PSRAM readback при 16 MHz; релизный Pico W профиль держит PSRAM PIO path на 8 MHz. Это ограничение не
распространяется на `picocalc_pico2w`.

Если конкретная SD card или плата нестабильна на production default, можно временно собрать образ с меньшей частотой:

```text
LEXXOS_PICOCALC_SD_SPI_BAUDRATE=25000000
```

USB CDC console mirror включается автоматически для PicoCalc. Чтобы явно выключить или включить его:

```text
LEXXOS_PICO_USB_CDC_CONSOLE=off
LEXXOS_PICO_USB_CDC_CONSOLE=on
```

Распиновка P2 для первичного запуска PSRAM/audio зафиксирована в профиле платы по схеме. Через CMake cache остаются
только
консервативные скорости/длительности для проверки:

```text
LEXXOS_PICOCALC_PSRAM_SPI_BAUDRATE=1000000
LEXXOS_PICOCALC_AUDIO_BEEP_FREQUENCY_HZ=880
LEXXOS_PICOCALC_AUDIO_BEEP_DURATION_MS=120
```

Команда `board` должна показывать:

```text
id: picocalc_pico_w
processor: RP2040 / rp2040
```

## Прошивка через BOOTSEL

Подключите USB Pico W в режиме BOOTSEL и скопируйте образ:

```bash
cp build/minimal-picocalc_pico_w/src/lexxos.uf2 /Volumes/RPI-RP2/
sync
```

Также можно использовать helper script:

```bash
tools/flash-picocalc-pico-w-uf2.sh
```

После запуска LexxOS команда shell `bootsel` может вернуть Pico W в USB BOOTSEL loader без физического нажатия BOOTSEL:

```text
lexx:/> bootsel
```

## Smoke-test после прошивки

Базовая последовательность проверок:

```text
lexx:/> board
lexx:/> version
lexx:/> i2c-buses
lexx:/> i2c-scan
lexx:/> storage
lexx:/> sd-debug show
lexx:/> sd-baud
lexx:/> sd-test
lexx:/> fs-smoke
lexx:/> sd-bench
lexx:/> picocalc-info
lexx:/> picocalc-info --direct-i2c
lexx:/> picocalc-sb
lexx:/> picocalc-sb --poll 2
lexx:/> picocalc-sb --lcd 40
lexx:/> backlight
lexx:/> backlight lcd 40
lexx:/> backlight kbd 10
lexx:/> backlight kbd 64
lexx:/> power
lexx:/> devices
lexx:/> display-info
lexx:/> display-test
lexx:/> display-text 0 0 2 green black LexxOS PicoCalc
lexx:/> display-power
lexx:/> display-power off
lexx:/> display-power on
lexx:/> display-power sleep
lexx:/> display-power on
lexx:/> display-power off
press any PicoCalc key to wake display
lexx:/> display-power sleep
press any PicoCalc key to wake display
lexx:/> display-power timeout
lexx:/> display-power timeout 300
lexx:/> display-power timeout 60 sleep
wait 60 seconds without input, then press any PicoCalc key to wake display
lexx:/> display-power timeout off
lexx:/> input-test picocalc-keyboard
lexx:/> keyboard-live -s 30 -i 16 picocalc-keyboard
lexx:/> edit /tmp/picocalc-input.txt
check LCD scrollback hotkeys: Shift/Ctrl/Sym/Alt+Up/Down and Sym/Alt+Home/End
lexx:/> console-info
check top status bar time, battery percent and status updated timestamp
lexx:/> rtc-status
lexx:/> rtc-get
lexx:/> rtc-set 2026-06-23 14:30:00
connect USB CDC terminal and run: help
lexx:/> psram-info
expect JEDEC ID 00 00 00 0D 5D 53 31 76 on confirmed PicoCalc hardware
lexx:/> psram-test
expect ok restored=yes
lexx:/> beep
expect left channel tone, short pause, then right channel tone
lexx:/> audio
expect tone=yes and mix=yes voices=6
copy a MIDI file to SD, for example /mnt/sd/midi/example.mid
lexx:/> mplayer /mnt/sd/midi/example.mid
expect polyphonic PWM playback and finished statistics
lexx:/> wifi-scan
ожидается список видимых сетей с SSID/channel/RSSI/auth
lexx:/> wifi-scan --raw
ожидаются исходные записи BSSID/channel/RSSI/auth для диагностики первичного запуска
lexx:/> wifi-config show
ожидается статус сохранённой конфигурации без вывода пароля
lexx:/> wifi-status
после auto-connect или ручного wifi-connect ожидается link/IP/DNS статус
lexx:/> ping --count 1 pool.ntp.org
ожидается DNS resolve и один ICMP ответ
lexx:/> rtc-sync --tz +07:00
ожидается NTP query и запись выбранного RTC backend
lexx:/> bootsel
```

`poweroff` проверяйте отдельно, только когда устройство можно выключить:

```text
lexx:/> poweroff
```

Ожидаемое безопасное поведение при активном `picocalc-keyboard`:

- `picocalc-info` показывает service cached snapshot и строку `direct-i2c: no`;
- `picocalc-info --direct-i2c` отказывается от прямого I2C-доступа и всё равно показывает service cached snapshot;
- `picocalc-sb` показывает service cached snapshot, а не читает southbridge-регистры напрямую;
- `picocalc-sb --poll 2` не читает FIFO напрямую и выводит предупреждение;
- `picocalc-sb --lcd 40`, `--kbd` и `--reset` отказываются выполнять direct write, пока клавиатура владеет southbridge;
- `backlight lcd 40` выполняет safe service-backed write;
- `backlight kbd 10` выполняет safe service-backed write; для factory firmware accepted level будет `32`, для JCS
  firmware это native step `1`;
- `backlight kbd 64` выполняет safe service-backed write без повреждения keyboard FIFO;
- `power` показывает состояние питания через southbridge service;
- `sd-baud` показывает stable production active baud `50000000 Hz`;
- `sd-test` пишет `/mnt/sd/LEXTEST.TXT`, читает payload обратно и удаляет test file;
- `fs-smoke` проходит long-name write/append/read/copy/rename/stat/remove/sync checks на `/mnt/sd`;
- `sd-bench` проходит raw read и FAT write/read/verify без ошибок;
- card-detect hot-plug печатает `sd card: removed`, unmount `/mnt/sd`, затем после вставки монтирует `/dev/sd0` на
  `/mnt/sd`;
- `devices` показывает registered service `picocalc-southbridge` с resource status `free` вне активной команды;
- `display-test` и `display-text` временно переводят LCD mirror в graphics mode на время отрисовки, а затем возвращают
  text mode для ответа команды;
- `display-power off` и `display-power sleep` меняют режим LCD controller и гасят LCD/keyboard backlight через
  southbridge service;
- `display-power on` возвращает отрисовку и восстанавливает последние LCD/keyboard backlight levels;
- первый `KeyDown` при `off`/`sleep` будит display и не передаётся дальше в shell;
- `display-power timeout` показывает `/mnt/sd/.lexx/display-power.conf`;
- `display-power timeout 60 sleep` сохраняет idle timeout, после 60 секунд бездействия переводит display в `sleep`,
  гасит LCD/keyboard backlight и пробуждается первым `KeyDown`;
- `edit /tmp/picocalc-input.txt` принимает ввод с PicoCalc keyboard через `picocalc-input`, стрелки двигают курсор, а
  `Escape` внутри editor виден как настоящий ESC; LCD editor redraw не печатает ANSI-последовательности как текст,
  показывает `^`/`~`, использует всю высоту текстовой области и не скроллит экран при перерисовке status line;
- LCD scrollback hotkeys на реальном PicoCalc: `Shift`/`Ctrl` + `Up`/`Down` двигают историю на строку, `Sym`/`Alt` +
  `Up`/`Down` двигают на страницу по фактической высоте LCD, `Sym`/`Alt` + `Home`/`End` переходят к началу истории или
  live-tail;
- top LCD status bar показывает время слева и battery percent/charging справа из cached `picocalc-southbridge` snapshot,
  а `console-info` выводит `status bar`, `status updated`, `status time` и `status battery`;
- `rtc-status`/`rtc-get`/`rtc-set` сохраняют общий RTC contract; на JCS firmware `auto` выбирает
  `picocalc-southbridge`, а `LEXXOS_FEATURE_DS3231_RTC` по умолчанию выключен для PicoCalc;
- USB CDC terminal определяется как `LexxOS PicoCalc`, показывает тот же shell output, принимает команды параллельно с
  UART, а LCD/PicoCalc keyboard остаются primary standalone interactive path;
- `psram-info` печатает PSRAM wiring из схемы платы и читает JEDEC ID `00 00 00 0D 5D 53 31 76` через conservative
  bitbang SPI probe без DMA;
- `psram-test` подтверждён на реальном PicoCalc: стандартные 256 байт и `--address 0x100 --length 128` завершаются
  `ok restored=yes`;
- `beep` подтверждён на реальном PicoCalc: PWM tone сначала на left GP26, затем на right GP27; аппаратный smoke 0.0.7
  дал несколько слышимых звуковых сигналов;
- `audio` предоставляет primary `picocalc-audio` с tone и software PWM tone mix на left/right pins;
- `mplayer` читает Standard MIDI files из VFS и использует `AudioDevice` mixer для первого полифонического playback;
- `wifi-scan` подтверждён на реальном PicoCalc: радиомодуль CYW43 инициализируется, обычный режим печатает
  объединённый список `SSID/channel/RSSI/auth`, а `--raw` печатает BSSID-записи для диагностики первичного запуска;
- `wifi-config`, boot `wifi-autoconnect`, `wifi-connect`, `wifi-status`, `ping` и `rtc-sync` образуют первый
  TCP/IP/LwIP path; hardware smoke 0.0.7 подтвердил auto-connect, DNS и ICMP, а `rtc-sync` получил source fix для
  PicoCalc JCS RTC write acknowledgement;
- `poweroff` выполняет VFS sync/unmount и затем запрашивает выключение через southbridge service; Pico 2 W smoke
  2026-07-08 подтвердил фактическое выключение, а старый Pico W smoke видел JCS-dependent `southbridge request failed`.

## Текущий статус поддержки

Подробности по southbridge ownership, cached snapshot, keyboard FIFO и безопасной диагностике вынесены в
[`docs/picocalc-southbridge.md`](../picocalc-southbridge.md).
Проверенная раскладка клавиатуры описана в [`docs/picocalc-keyboard-layout.md`](../picocalc-keyboard-layout.md).

Работает:

- board profile для Raspberry Pi Pico W;
- UART0 как аварийный диагностический канал;
- USB CDC console mirror как удобный debug channel поверх того же shell;
- SPI SD card с card-detect, FAT automount и default 50 MHz transfer baud;
- LCD 320x320 как `ili9488-spi` display device;
- board display config использует нейтральный `spiTftDisplay`/`SpiTftDisplayPins`, а конкретный драйвер PicoCalc
  регистрируется как `ili9488-spi`;
- LCD mirror console со scrollback и hardware-verified line/page/top/end hotkeys;
- top LCD status bar с RTC time слева, battery percent, battery icon и charging indicator через cached
  `picocalc-southbridge` snapshot;
- text/graphics handoff через `ConsoleMirrorMode::Graphics`: display diagnostics временно приостанавливают LCD mirror
  на время отрисовки и автоматически возвращают text mode;
- универсальная команда `display-power [on|off|sleep]`, поддержанная TFT-драйверами `ili9488-spi` и `st7735-spi`;
- PicoCalc display-power policy поверх `display-power`: LCD/keyboard backlight гаснут при `off`/`sleep`,
  восстанавливаются при `on`, а PicoCalc keypress будит display;
- idle timeout для display через `display-power timeout`, файл настроек
  `/mnt/sd/.lexx/display-power.conf`, runtime default `300s off`;
- PicoCalc keyboard как `picocalc-keyboard` input device;
- background `picocalc-input` task, который прокидывает PicoCalc keyboard в общий console input stream;
- shell и terminal apps получают ввод с клавиатуры PicoCalc при сохранении UART fallback;
- shell также доступен через USB CDC terminal, когда host открыл CDC port;
- короткий input-device lock вокруг PicoCalc input poll, чтобы background bridge не конкурировал с
  `keyboard-live`/`input-test`;
- retry/backoff при interactive input lock в `input-test`, `keyboard-live` и `keyboard-raw`;
- устойчивое чтение клавиатуры через southbridge FIFO с фильтрацией повторных scan-code;
- extended cached southbridge snapshot, публикуемый клавиатурным драйвером через `PicoCalcSouthbridgeService`;
- безопасная диагностика cached snapshot через `picocalc-info` и `picocalc-sb` без direct I2C при активной клавиатуре;
- provider-owned southbridge architecture: `picocalc-keyboard` владеет destructive FIFO и отдельный runtime-service
  facade экспортирует `picocalc-southbridge` для battery/backlight/power клиентов;
- service-level resource lock для `picocalc-southbridge`; `devices` показывает `free`/`busy`, priority и owner для
  southbridge service так же, как для input/display/block resources;
- универсальные команды `backlight`, `power` и `poweroff`, которые используют PicoCalc southbridge service path;
- battery snapshot, charging flag и процент заряда в `power`/`picocalc-info`;
- LCD backlight и keyboard backlight через `backlight lcd`/`backlight kbd`;
- строгая проверка southbridge write-response по register id с диагностикой `state=refused` для неверно подтверждённых
  write-команд;
- clean `poweroff`: VFS sync и unmount активных filesystem mount points; factory southbridge power-off path подтверждён,
  Pico 2 W smoke 2026-07-08 подтвердил JCS software request до фактического выключения;
- сохранение и восстановление уровней подсветки через `backlight save`/`backlight restore` на SD card, включая
  автоматический restore после boot SD automount;
- PicoCalc SD/FAT smoke на реальном устройстве: `sd-test`, `fs-smoke`, `sd-bench`, `sd-baud`, hot-plug automount;
- read-only `sd-speed-scan 60000000 10000 1024` подтвердил stable raw/FAT read path на 60 MHz; production default
  остаётся 50 MHz, как у `dev_pico`;
- проверенная PicoCalc keyboard layout coverage для `A..Z`, цифр, базовой ASCII punctuation, `F1..F10`, `Break`,
  `Home`, `End`, `Delete` и `Insert`;
- autorepeat для удерживаемых `Left`, `Right`, `Up`, `Down`, `Backspace` и `Space`;
- защита от залипших non-repeatable scan-code через stale-release timeout;
- переход в BOOTSEL через команду `bootsel`;
- PSRAM pinout по схеме платы и команда `psram-info` для чтения JEDEC ID через conservative bitbang SPI probe;
- audio beep pinout по схеме платы и команда `beep`, которая проверяет left GP26, затем right GP27;
- CYW43 Wi-Fi capability, `wifi-scan`, `wifi-config`, `wifi-connect`, `wifi-status`, `ping` и boot
  `wifi-autoconnect`; `rtc-sync --tz +07:00` подтверждён на PicoCalc Pico 2 W.
- Wi-Fi-enabled Pico boards держат CPU clock не выше 250 MHz. CMake теперь останавливает конфигурацию, если
  `LEXXOS_PICO_SYS_CLOCK_KHZ` явно задан выше `250000` для `dev_pico2w`, `picocalc_pico_w` или `picocalc_pico2w`:
  PicoCalc Pico 2 W SMP/Game Boy smoke 2026-07-11 подтвердил, что 266 MHz ломает CYW43 connect/scan path, а 250 MHz
  проходит boot `wifi-autoconnect`, `wifi-status`, `wifi-scan` и `ping`.
- tight-RAM release profile для RP2040: 88 KiB FreeRTOS heap, PSRAM 8 MHz cap, без Game Boy/MIDI/GPIO analyzer, с lazy
  display benchmark heap buffers вместо static `.bss` или descriptor-level quota.

Ограничения текущей реализации:

- UART остаётся аварийным каналом для bring-up и отладки; USB CDC mirror является удобным, но не обязательным
  дополнительным debug path.
- Game Boy, MIDI `mplayer` и GPIO analyzer не входят в релизный Pico W образ 0.0.7 из-за ограниченной SRAM RP2040.
  Для экспериментов нужен отдельный override и повторная проверка map/heap на конкретной сборке.
- `display-test` pattern mode не резервирует benchmark workspace. `display-test --bench` лениво выделяет RGB565 chunk
  buffers из heap: дефолтный `--rows 16` требует около 20 KiB для full-width two-buffer transports, а явный
  `--rows 64` может потребовать около 80 KiB. При нехватке heap ожидаемый отказ —
  `display-test: not enough heap for benchmark workspace`.
- LCD renderer является текстовым mirror-слоем, а не полноценным оконным/UI subsystem.
- Layout mapping вынесен в `picocalc_keyboard_layout.cpp` и покрыт host contract tests. Печатные символы проходят через
  `character`, а проверенное покрытие раскладки зафиксировано в
  [`docs/picocalc-keyboard-layout.md`](../picocalc-keyboard-layout.md).
- Autorepeat реализован в драйвере с задержкой 500 ms и интервалом 60 ms. Повторяются только стрелки, Backspace и Space.
  Печатные символы, Enter, Escape, Soft1 и Soft2 намеренно не повторяются, чтобы потерянный `key-up` не мог забить shell
  потоком символов.
- Modifiers отслеживаются внутри драйвера, но полноценная таблица сочетаний Sym/Shift ещё не реализована.
- В runtime shell mode `Escape` закрывает активный LCD scrollback, а вне scrollback работает как shell cancel
  (`Ctrl-C`). Во время запущенной программы `Escape` передаётся как настоящий ESC для `terminal::readKey()`.
- На проверенной прошивке PicoCalc power button не появляется как keyboard FIFO event, поэтому `KeyCode::Power` не
  маппится в клавиатурном драйвере. Если приложениям понадобится physical power button event, нужен отдельный
  PMU/power-event source.
- Wi-Fi поддержка вышла за пределы scan-only: есть сохранение credentials, boot auto-connect, ручное подключение,
  status, ICMP ping и NTP request path в `rtc-sync`. Ограничение этапа — это ещё не полноценный сетевой стек приложений:
  пароль хранится на SD в открытом виде, retry/roaming policy минимальная, а `rtc-sync` после source fix нужно повторно
  проверить на реальном PicoCalc.
- Bluetooth только объявлен как CYW43 capability в board profile; runtime API и use case ещё не добавлены.
- PSRAM пока используется только для `psram-info`/JEDEC ID и подтверждённого малодиапазонного `psram-test`;
  allocator/cache pool и quad/PIO backend ещё не реализованы.
- Audio имеет `AudioDevice` и первый software PWM mixer для MIDI, но качество синтеза на реальном PicoCalc,
  DMA/PIO/I2S backend и MT-32-like envelopes/partials ещё не закрыты.
- В обычной PicoCalc-сборке прямые `picocalc-sb --lcd`, `--kbd` и `--reset` заблокированы при активном
  `picocalc-keyboard`; используйте универсальные `backlight`, `power` и `poweroff`.
- 60 MHz пока считается performance/test ceiling для PicoCalc: read path подтверждён, но stable production default
  остаётся 50 MHz.

## Клавиатура PicoCalc

Клавиатура PicoCalc обслуживается STM32 southbridge по I2C. Драйвер LexxOS читает события из FIFO-регистра southbridge и
отдаёт их как `InputEvent`.

Особенности текущей реализации:

- register `keyboard-state` используется только как подсказка: на некоторых прошивках southbridge счётчик FIFO может
  быть равен нулю даже при наличии событий;
- если счётчик FIFO не сообщает события, драйвер делает короткий консервативный probe FIFO;
- чтение прекращается на первом `0x0000`, чтобы не вычитывать устаревшие значения;
- состояние нажатых scan-code хранится отдельно для всех 256 кодов;
- повторный `key-down` для уже нажатого scan-code фильтруется;
- `key-up` очищает состояние scan-code и отменяет autorepeat;
- non-repeatable scan-code автоматически очищается через stale-release timeout, если `key-up` потерялся;
- `stale-event-releases` и `stale-scan-code-releases` в диагностике показывают, сколько таких авто-сбросов выполнил
  драйвер;
- autorepeat генерирует обычные `KeyDown` события только для безопасного набора клавиш: стрелки, Backspace и Space.

Проверка клавиатуры:

```text
lexx:/> keyboard-live -s 30 -i 16 picocalc-keyboard
```

Raw проверка новых firmware/layout variants:

```text
lexx:/> keyboard-live --full -s 30 -i 16 picocalc-keyboard
```

Проверка autorepeat:

```text
lexx:/> keyboard-live -s 10 -i 16 picocalc-keyboard
```

Во время проверки удерживайте `Left`, `Right`, `Up`, `Down`, `Backspace` и `Space`. Ожидаемое поведение: после короткой
паузы идут повторные `key-down`, а после отпускания клавиши `pressed` возвращается в `<none>`.

Печатные символы, цифры и `Enter` сейчас не повторяются намеренно. Это защита от ситуации, когда STM32/southbridge или
FIFO теряет `key-up`, а software-autorepeat начинает бесконечно печатать в shell.

## Southbridge diagnostics

При активном `picocalc-keyboard` southbridge принадлежит клавиатурному драйверу. Диагностические команды не должны
напрямую читать регистры STM32 southbridge, потому что прямое чтение может забрать событие из FIFO или нарушить порядок
обработки `key-up`.

### `picocalc-info`

```text
lexx:/> picocalc-info
```

Показывает cached snapshot, который публикует клавиатурный драйвер:

```text
PicoCalc southbridge info
source: picocalc-southbridge service cached snapshot
direct-i2c: no
keyboard-ready: yes
snapshot-ms: ...
snapshot-full-refresh-ms: ...
snapshot-last-refresh-ok: yes
snapshot-age-ms: ...
snapshot-fields: ...
snapshot-updates: ...
snapshot-failures: 0
stale-event-releases: ...
stale-scan-code-releases: ...
battery: ...
lcd-backlight: ...
keyboard-backlight: ...
power-off-supported: ...
```

`picocalc-info --direct-i2c` при активной клавиатуре намеренно отказывается от direct I2C и остаётся в safe
cached-snapshot режиме. Deprecated `--force` остаётся alias для direct-I2C диагностики без active keyboard service.

### `picocalc-sb`

```text
lexx:/> picocalc-sb
```

При активной клавиатуре показывает тот же safe cached snapshot:

```text
PicoCalc southbridge
source: picocalc-southbridge service cached snapshot
direct-i2c: no
keyboard-ready: yes
snapshot-ms: ...
snapshot-full-refresh-ms: ...
snapshot-last-refresh-ok: yes
snapshot-fields: ...
snapshot-updates: ...
snapshot-failures: 0
stale-event-releases: ...
stale-scan-code-releases: ...
battery: ...
lcd-backlight: ...
keyboard-backlight: ...
power-off-supported: ...
```

Если указать устаревший `--poll`, команда не будет читать FIFO и выведет предупреждение. Для безопасной проверки
клавиатуры используйте `keyboard-live` или `input-test`.

```text
lexx:/> picocalc-sb --poll 2
```

`--lcd`, `--kbd` и `--reset` при активной клавиатуре не выполняются:

```text
lexx:/> picocalc-sb --lcd 40
picocalc-sb: direct write request refused while picocalc-keyboard is active
```

Прямой режим чтения/записи southbridge-регистров доступен только через `--direct-i2c`, только в конфигурации, где
`picocalc-keyboard` не зарегистрирован и никто не владеет FIFO клавиатуры. В обычной сборке PicoCalc используйте только
cached snapshot.

### `backlight`, `display-power`, `power`, `poweroff`

Для обычной работы используйте универсальные команды:

```text
lexx:/> backlight
lexx:/> backlight lcd 40
lexx:/> backlight kbd 64
lexx:/> backlight save
lexx:/> backlight save lcd
lexx:/> backlight restore
lexx:/> backlight load
lexx:/> backlight restore lcd
lexx:/> power
lexx:/> display-power
lexx:/> display-power off
lexx:/> display-power on
lexx:/> display-power sleep
lexx:/> display-power timeout
lexx:/> display-power timeout 300
lexx:/> display-power timeout 60 sleep
lexx:/> display-power timeout off
lexx:/> poweroff
```

`backlight save` использует файл `/mnt/sd/.lexx/backlight.conf`. `backlight restore`/`backlight load`
применяет его вручную, а boot runtime автоматически применяет этот файл после успешного SD automount. Команда сохраняет
универсальный пользовательский контракт `0..255`, но внутри выбирает firmware-specific диапазон: factory keyboard
backlight квантуется в `0`, `32`, ..., `224`, а JCS использует native steps LCD `0..9` и keyboard `0..3`.

`display-power` управляет режимом LCD controller через общий `DisplayDevice` API, а на PicoCalc дополнительно применяет
board policy через `picocalc-southbridge` service. `off` и `sleep` гасят LCD/keyboard backlight; `on` возвращает
отрисовку и восстанавливает последние уровни подсветки. Первый `KeyDown` с PicoCalc keyboard при выключенном display
пробуждает экран и не передаётся дальше в shell.

`display-power timeout <seconds> [off|sleep]` сохраняет idle timeout в
`/mnt/sd/.lexx/display-power.conf`. Значение `off` отключает автоотключение. Пока config не найден,
используется runtime default `300s off`.

`poweroff` синхронизирует VFS, размонтирует активные filesystem mount points и затем запрашивает выключение через
southbridge service. Factory path подтверждён на реальном PicoCalc. Для JCS firmware команда пробует `REG_ID_OFF`
sleep/off request (`0x40`): старый Pico W smoke видел `southbridge request failed` после успешного sync/unmount, а
Pico 2 W smoke 2026-07-08 подтвердил фактическое выключение. Полный PMIC shutdown (`0x80`) команда не использует.

```text
lexx:/> poweroff
```

Документ соответствует LexxOS 0.0.7.
