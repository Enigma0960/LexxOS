# PicoCalc JCS keyboard/southbridge firmware

Ссылка на прошивку [picocalc_BIOS](https://git.jcsmith.fr/jackcartersmith/picocalc_BIOS).
Зеркало [picocalc_southbridge](https://github.com/shtirlic/picocalc_southbridge)

Документ фиксирует проверенный сценарий использования альтернативной JCS-прошивки STM32 southbridge на ClockworkPi
PicoCalc с LexxOS 0.0.7.

JCS-прошивка не является заводской прошивкой ClockworkPi. Поэтому профиль LexxOS по умолчанию остаётся заводским:

```text
LEXXOS_PICOCALC_KEYBOARD_FIRMWARE=factory
```

Профиль `jcs` нужно включать явно только после осознанной перепрошивки STM32 southbridge:

```text
LEXXOS_PICOCALC_KEYBOARD_FIRMWARE=jcs
```

## Что уже проверено

На реальном PicoCalc подтверждено:

```text
JCS southbridge firmware     ok
LexxOS profile jcs           ok
keyboard I2C communication   ok
all keys                     ok
keyboard events              ok
picocalc-keyboard input      ok
rtc-get/rtc-set/rtc-status   ok
physical power button keeps RTC date/time ok
```

Проверка всех клавиш показала, что клавиатура отвечает корректно и работает как ожидается. RTC support в LexxOS
использует тот же универсальный command contract `rtc-get`/`rtc-set`/`rtc-status`, а backend выбирается внутри команды.
Software `poweroff` пока остаётся known limitation для JCS: VFS sync/unmount проходит, но southbridge request может
вернуть failure.

## Сборка LexxOS

Обычная сборка для заводской прошивки не требует дополнительных параметров:

```bash
cmake --preset debug-picocalc-pico-w
cmake --build --preset lexxos-picocalc-pico-w
```

Для JCS-прошивки нужно явно выбрать профиль:

```bash
cmake -S . -B build/configurator/pico \
  -DLEXXOS_PLATFORM=pico \
  -DLEXXOS_BOARD=picocalc_pico_w \
  -DLEXXOS_PICOCALC_KEYBOARD_FIRMWARE=jcs

cmake --build build/configurator/pico -j
```

Проверка CMake cache:

```bash
grep -E "LEXXOS_PICOCALC_KEYBOARD_FIRMWARE|LEXXOS_PICOCALC_SYSTEM_I2C_BAUDRATE" \
  build/configurator/pico/CMakeCache.txt
```

Ожидаемый вывод для JCS-профиля:

```text
LEXXOS_PICOCALC_KEYBOARD_FIRMWARE:STRING=jcs
LEXXOS_PICOCALC_SYSTEM_I2C_BAUDRATE:STRING=
```

Пустое значение `LEXXOS_PICOCALC_SYSTEM_I2C_BAUDRATE` означает, что board profile сам выбирает безопасное значение для
выбранного firmware profile:

```text
factory -> 400000 Hz
jcs     -> 10000 Hz
```

При необходимости скорость можно переопределить явно:

```text
LEXXOS_PICOCALC_SYSTEM_I2C_BAUDRATE=50000
LEXXOS_PICOCALC_SYSTEM_I2C_BAUDRATE=100000
LEXXOS_PICOCALC_SYSTEM_I2C_BAUDRATE=400000
```

Для первичной диагностики JCS рекомендуется оставлять автоматический режим, то есть 10 kHz.

## Прошивка STM32 southbridge

Проверенный порядок прошивки JCS BIOS:

```text
1. Отключить питание PicoCalc.
2. Отключить или вынуть батареи.
3. Перевести STM32 в bootloader mode: SW701-1 = ON.
4. Подключить USB к motherboard PicoCalc.
5. Включить плату кнопкой питания.
6. Подключиться к STM32 через STM32CubeProgrammer UART.
7. Выполнить полный erase STM32: Full chip erase / Erase all sectors.
8. Прошить build/picocalc_BIOS_jcs.hex или build/picocalc_BIOS_jcs.bin.
9. Дождаться Verify OK.
10. Отключить питание.
11. Вернуть SW701-1 = OFF.
12. Полностью обесточить устройство на 10-20 секунд.
13. Включить PicoCalc в обычном режиме.
```

Для `.bin` используется адрес:

```text
0x08000000
```

Для `.hex` адрес обычно уже содержится в файле.

## Важное правило: делать полный erase

При переходе с заводской прошивки на JCS нужно делать именно полный erase STM32, а не только запись `.hex`/`.bin`.

Подтверждённая проблема:

```text
STM32_Programmer_CLI -e all падал с segmentation fault после строки Mass erase ...
последующая запись .bin/.hex проходила успешно
CubeProgrammer стирал только страницы прошивки [0..36]
служебная область Flash / EEPROM emulation оставалась от старой прошивки
JCS-прошивка стартовала неполностью
Pico включался, но I2C slave southbridge не отвечал
LexxOS не видел клавиатуру
```

Симптомы в LexxOS:

```text
[picocalc-keyboard] communication failure: stage=initialize-keyboard-state-read
ready=no
comm-failures=...
consecutive=...
sb-reset-attempts=...
sb-reset-successes=0
last-state=0x00
fifo=0
raw=0x00000000
snapshot-ok=no
```

`picocalc-info` в таком состоянии показывает:

```text
picocalc-info: skipped while picocalc-keyboard owns the southbridge I2C device
picocalc-info: direct reads are disabled while normal keyboard input is active
picocalc-info: use power/backlight for safe service-backed power and backlight requests
picocalc-info: no cached keyboard-state snapshot is available yet
```

После полного erase STM32 и повторной прошивки JCS southbridge поднялся корректно, I2C-обмен восстановился, клавиатура
заработала.

## Bootloader и нормальный режим

Если `SW701-1 = ON` и плата включена, STM32 должен отвечать в ROM bootloader через UART. Пример успешного подключения:

```text
Activating device: OK
Chip ID: 0x410
BootLoader protocol version: 2.2
Device name : STM32F101/F102/F103 Medium-density
Device CPU  : Cortex-M3
```

После прошивки нужно вернуть `SW701-1 = OFF` и сделать полный power cycle. В обычном режиме STM32 bootloader уже не
обязан отвечать. Ошибка вида ниже в обычном режиме нормальна:

```text
Error: Activating device: KO
```

Она означает, что STM32 не находится в ROM bootloader, а стартует из Flash.

## Проблема с выбором serial-port на macOS

На macOS могут появляться разные порты; `XXXX` ниже — условный идентификатор,
который нужно заменить фактическим:

```text
/dev/cu.usbmodemXXXX
/dev/cu.usbserial-XXXX
/dev/cu.debug-console
```

В исходном прогоне STM32 bootloader отвечал через USB-UART; имя порта здесь обезличено:

```text
/dev/cu.usbserial-XXXX
```

Порт `/dev/cu.usbmodemXXXX` открывался, но STM32 не отвечал ACK:

```text
Serial Port /dev/cu.usbmodemXXXX is successfully opened.
Timeout error occured while waiting for acknowledgement.
Error: Activating device: KO.
```

Это было не проблемой прошивки, а неверным портом или неверным состоянием платы.

Проверка появления нужного порта:

```bash
ls /dev/cu.* > /tmp/ports-before.txt
# отключить и снова подключить PicoCalc в STM32 bootloader mode
ls /dev/cu.* > /tmp/ports-after.txt
diff /tmp/ports-before.txt /tmp/ports-after.txt
```

## Проблема с питанием платы

STM32 bootloader отвечал только после того, как PicoCalc был не просто подключён к USB, а действительно включён кнопкой
питания.

Симптом, если плата не включена или находится не в том режиме:

```text
Serial Port ... is successfully opened.
Timeout error occured while waiting for acknowledgement.
Error: Activating device: KO.
```

Правильная последовательность:

```text
SW701-1 = ON
USB подключён
плата включена кнопкой питания
```

## Проблема с STM32_Programmer_CLI на macOS

На macOS `STM32_Programmer_CLI` из STM32CubeIDE успешно подключался к STM32, но падал на `-e all`:

```text
Mass erase ...
zsh: segmentation fault  "$STM32_CLI" -c port="$PORT" br=115200 -e all ...
```

Обходные варианты:

```text
1. Выполнить Full chip erase через GUI STM32CubeProgrammer.
2. Не использовать отдельный -e all, если полный erase уже сделан через GUI.
3. Для записи использовать .hex или .bin с адресом 0x08000000.
4. Как запасной вариант использовать stm32flash.
```

Запись без отдельного `-e all` после полного erase:

```bash
"$STM32_CLI" \
  -c port="$PORT" br=115200 \
  -w build/picocalc_BIOS_jcs.bin 0x08000000 \
  -v
```

Пример успешной записи:

```text
File download complete
Verifying...
Download verified successfully
```

## Диагностика LexxOS после прошивки

Проверить, что выбран профиль JCS:

```text
lexx:/> board
```

Ожидаемо:

```text
i2c: index=1 sda=6 scl=7 baud=10000
```

Проверить состояние southbridge:

```text
lexx:/> picocalc-info
```

Ожидаемые признаки исправной связи:

```text
keyboard-ready: yes
snapshot-last-refresh-ok: yes
snapshot-failures: 0
```

Проверенные JCS RTC команды:

```text
lexx:/> rtc-status
lexx:/> rtc-get
lexx:/> rtc-set 2026-06-23 14:30:00
```

Ожидаемые признаки:

```text
source: picocalc-southbridge
firmware: type=0xCA
```

Пример подтверждённого вывода:

```text
lexx:/> rtc-get
source: picocalc-southbridge
rtc: 2026-06-23 18:06:50
weekday: 2 (1=Monday, 7=Sunday)
lexx:/> rtc-status
source: picocalc-southbridge
status: 0x01
firmware: type=0xCA version=0.9 jcs=yes
rtc alarm: enabled=yes rearm=no
```

В PicoCalc/JCS-сборке `LEXXOS_FEATURE_DS3231_RTC` по умолчанию выключен: DS3231 нет на штатной плате, а RTC берётся из
JCS southbridge. Если отдельный DS3231 всё-таки подключён, его можно вернуть отдельным feature-флагом и выбрать
`--source ds3231`.

Проверить клавиатуру:

```text
lexx:/> keyboard-live -s 30 -i 16 picocalc-keyboard
```

Для полной проверки раскладки:

```text
lexx:/> keyboard-live --full -s 30 -i 16 picocalc-keyboard
```

## Почему i2c-scan может вводить в заблуждение

JCS southbridge работает по register protocol: сначала мастер пишет номер регистра, затем читает ответ. Обычный scanner
может делать простое чтение адреса без предварительной записи регистра. Поэтому отсутствие `0x1F` в `i2c-scan` не всегда
доказывает неисправность southbridge.

Главные проверки для PicoCalc:

```text
picocalc-info
keyboard-live -s 30 -i 16 picocalc-keyboard
реальный ввод клавиш в shell/editor
```

Если `picocalc-keyboard` сообщает постоянные `communication failure`, проблема не в раскладке клавиш, а в базовом
I2C-обмене с STM32.

## Known issue: одиночный `update-keyboard-fifo-read`

На JCS-профиле могут периодически появляться одиночные строки:

```text
[picocalc-keyboard] communication failure: stage=update-keyboard-fifo-read ... ready=yes ...
consecutive=1 ... snapshot-ok=yes
```

Это известное неблокирующее предупреждение, а не отказ устройства, если одновременно выполняются условия:

- `ready=yes`;
- `consecutive=1` или счётчик не растёт серией;
- `snapshot-ok=yes`;
- ввод с клавиатуры продолжает работать.

Такое поведение подтверждено 2026-07-11 на PicoCalc Pico 2 W в 2-core SMP и в контрольной 1-core сборке. В обоих
режимах SD/FAT smoke, shell и `gbemu` продолжали работать, а выход из Game Boy по Start+Select срабатывал. Поэтому
одиночный `update-keyboard-fifo-read` нужно трактовать как known issue/diagnostic noise.

Диагностировать это как настоящую проблему нужно только если ошибки становятся постоянными, `consecutive` растёт,
появляется `ready=no` или `snapshot-ok=no`, либо реально пропадают события клавиатуры.

## RTC, status bar и power/backlight

LexxOS использует JCS register protocol из зеркала
[`shtirlic/picocalc_southbridge`](https://github.com/shtirlic/picocalc_southbridge):

```text
0x00 firmware type, JCS = 0xCA
0x01 firmware version
0x13 RTC config
0x14 RTC date
0x15 RTC time
0x16 RTC alarm date
0x17 RTC alarm time
```

Status bar берёт время из cached `picocalc-southbridge` snapshot и показывает его слева. `console-info` должен печатать:

```text
status time: HH:MM:SS
```

`backlight` остаётся универсальной командой, но JCS firmware хранит значения как native steps:

```text
LCD backlight:      0..9
keyboard backlight: 0..3
```

Значения больше native max команда сжимает из пользовательского диапазона `0..255` вверх к ближайшей ступени. Поэтому
`backlight lcd 40` может вернуть `lcd=2`, а `backlight kbd 64` — `keyboard=1`; это нормальный JCS accepted value.

`poweroff` в JCS-профиле пробует `REG_ID_OFF=0x40`, то есть стандартный southbridge sleep/off request. На текущем
hardware smoke-test команда выполнила `sync` и unmount `/mnt/sd`, но southbridge request вернул failure. Физическая
кнопка питания при этом не сбросила дату и время JCS RTC. Полный PMIC shutdown (`0x80`) команда не использует, чтобы не
сбрасывать внутренний RTC/config контекст southbridge.

## Краткая таблица проблем

| Симптом                                                                  | Вероятная причина                                                                         | Решение                                                                              |
|--------------------------------------------------------------------------|-------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------------|
| `Timeout error occured while waiting for acknowledgement` при прошивке   | плата не включена, не тот порт или SW701-1 не в ON                                        | включить PicoCalc кнопкой питания, проверить SW701-1 и выбрать `/dev/cu.usbserial-*` |
| `STM32_Programmer_CLI` падает на `Mass erase`                            | ошибка CLI на macOS                                                                       | выполнить Full chip erase через GUI STM32CubeProgrammer                              |
| Запись `.hex/.bin` успешна, но клавиатура не работает                    | не очищена служебная Flash/EEPROM область STM32                                           | сделать полный erase STM32 и прошить заново                                          |
| `board` показывает 400 kHz при JCS                                       | LexxOS собран не с JCS-профилем или прошит старый UF2                                     | пересобрать с `LEXXOS_PICOCALC_KEYBOARD_FIRMWARE=jcs` и прошить свежий UF2           |
| `communication failure: initialize-keyboard-state-read`                  | STM32 southbridge не отвечает на register-read                                            | проверить полный erase, питание, SW701-1 OFF после прошивки, SDA/SCL GP6/GP7         |
| одиночный `communication failure: update-keyboard-fifo-read`             | transient FIFO read warning на JCS                                                        | known issue; не блокер при `ready=yes`, `consecutive=1`, `snapshot-ok=yes` и рабочем вводе |
| `i2c-scan` не находит `0x1F`, но клавиатура работает                     | scanner не является полным тестом register protocol                                       | использовать `picocalc-info` и `keyboard-live`                                       |
| `Error: Activating device: KO` после возврата SW701-1 OFF                | STM32 уже не в ROM bootloader                                                             | это нормально для штатного режима                                                    |
| `backlight` возвращает маленькие значения вроде `lcd=2` или `keyboard=1` | JCS firmware отдаёт native step, а не исходное `0..255`                                   | это нормально; `save`/`restore` сохраняют эти native значения                        |
| `poweroff` печатает `southbridge request failed` на JCS                  | software sleep/off request `0x40` не подтверждён текущей прошивкой/состоянием southbridge | пока использовать физическую кнопку питания; VFS sync/unmount до отказа выполняется  |

## Инварианты для дальнейшей поддержки

- `factory` остаётся значением по умолчанию.
- `jcs` включается только явно через CMake.
- Для JCS по умолчанию используется консервативная I2C-скорость 10 kHz.
- DS3231 выключен по умолчанию для PicoCalc; универсальные RTC-команды выбирают JCS southbridge backend через `auto`.
- JCS backlight accepted values являются native steps: LCD `0..9`, keyboard `0..3`.
- Software `poweroff` на JCS не считать подтверждённым, пока southbridge request возвращает failure.
- При переходе между прошивками southbridge делать полный erase STM32.
- После прошивки обязательно вернуть `SW701-1 = OFF`.
- После изменения SW701 или прошивки делать полный power cycle, а не только reboot RP2040.
- Проверку начинать с `picocalc-info` и `keyboard-live`, а не с `i2c-scan`.
- Не использовать direct I2C-доступ к keyboard FIFO при активном `picocalc-keyboard`.
- Одиночный `update-keyboard-fifo-read` с `ready=yes`, `consecutive=1`, `snapshot-ok=yes` и рабочим вводом считать
  known non-blocking warning, а не отказом клавиатуры.

## Связанные документы

```text
docs/boards/picocalc_pico_w.md
docs/picocalc-southbridge.md
docs/picocalc-keyboard-layout.md
```
