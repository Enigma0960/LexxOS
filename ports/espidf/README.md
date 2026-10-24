# ESP-IDF порт LexxOS

Этот каталог больше не является основным CMake-проектом для ESP32.

Начиная с версии 0.0.6, ESP-IDF сборка подключается через корневой
`CMakeLists.txt`, чтобы CLion мог работать с одним проектом и переключать
платформы через CMake profiles.

## Правильная сборка

Из корня репозитория:

```bash
. "$IDF_PATH/export.sh"

cmake --preset debug-esp32
cmake --build --preset lexxos-esp32
```

Прошивка:

```bash
cmake --build --preset lexxos-esp32-flash
```

Прошивка только приложения:

```bash
cmake --build --preset lexxos-esp32-app-flash
```

Для M5Stack Cardputer-Adv используется board profile `m5stack_cardputer_adv`. В текущем локальном bring-up окружении
user preset называется `user-espidf-m5stack_cardputer`, но выбирает именно Adv-плату:

```bash
cmake --fresh --preset user-espidf-m5stack_cardputer
cmake --build --preset user-espidf-m5stack_cardputer-build
ESPPORT=/dev/tty.usbmodemXXXX ESPBAUD=921600 cmake --build build/configurator/cardputer --target app-flash
```

Замените serial port на фактический ESP32-S3 USB-Serial/JTAG endpoint. На macOS после watchdog/reset `tty.usbmodem*`
может входить в ROM loader надёжнее, чем соответствующий `cu.usbmodem*`. Если CMake запущен под Rosetta на Apple
Silicon, preset должен использовать arm64 ESP-IDF shell/Python окружение (`LEXXOS_ESPIDF_EXPORT_SHELL_ARCH=arm64`),
иначе ESP-IDF Python wheels могут не загрузиться.

## Сборка из подкаталога

Если терминал открыт внутри `ports/espidf`, используйте wrapper из корня
репозитория:

```bash
../../tools/lexxos-cmake --preset debug-esp32
../../tools/lexxos-cmake --build --preset lexxos-esp32
```

Wrapper сам перейдёт в корень проекта и запустит CMake с корневыми presets.

## Что осталось в этом каталоге

Этот каталог всё ещё содержит данные ESP-IDF порта:

- `components/lexxos` — ESP-IDF component LexxOS;
- `sdkconfig.defaults` — настройки ESP-IDF по умолчанию;
- `partitions_m5stack_basic_v1_4mb.csv` — partition table для M5Stack Basic v1;
- `CMakePresets.legacy.json` — старые presets для справки.

Файл `CMakeLists.txt` оставлен только как защитный legacy entry point и
завершает конфигурацию с сообщением об ошибке, если этот каталог случайно
открыт как отдельный проект.

## CLion

В CLion нужно открывать только корень репозитория LexxOS. Профиль ESP32 должен
выбираться в общем списке CMake profiles как `debug-esp32`.

# LexxOS ESP-IDF port

Эта папка содержит отдельный ESP-IDF проект для запуска LexxOS на ESP32.
Первый поддерживаемый профиль платы — `m5stack_basic_v1`.

Текущий bring-up поднимает:

- ESP-IDF `app_main`;
- LexxOS runtime;
- FreeRTOS tasks поверх уже запущенного ESP-IDF scheduler;
- UART0 shell;
- базовый GPIO backend;
- VFS и `/tmp` tmpfs;
- кнопки A/B/C на передней панели M5Stack Basic;
- LCD 320x240;
- SD card SPI block device и FatFs automount в `/mnt/sd`;
- общий ESP-IDF SPI backend для будущих LCD/TF устройств;
- диагностические команды ядра и ESP32;
- встроенный LCD 320x240 через общую SPI-шину LCD/TF.

Меню выбора ROM и Game Boy остаются следующими отдельными патчами после стабилизации shell, кнопок, экрана и SD.

## Подготовка окружения

Перед сборкой в терминале загрузите окружение ESP-IDF. Пути `/path/to/...`
замените своими, а `XXXX` в примерах serial-порта — идентификатором своего устройства:

```bash
source /path/to/esp-idf/export.sh
```

Проверьте, что `idf.py` видит установленный IDF:

```bash
idf.py --version
```

## Сборка из терминала

```bash
cd /path/to/LexxOS/ports/espidf

idf.py \
  -B build/m5stack-basic-v1 \
  -DIDF_TARGET=esp32 \
  -DLEXXOS_PLATFORM=espidf \
  -DLEXXOS_BOARD=m5stack_basic_v1 \
  build
```

## Прошивка и monitor

Замените порт, если плата определилась иначе:

```bash
idf.py \
  -B build/m5stack-basic-v1 \
  -p /dev/cu.SLAB_USBtoUART \
  flash monitor
```

Для быстрой перезаписи только приложения:

```bash
idf.py \
  -B build/m5stack-basic-v1 \
  -p /dev/cu.SLAB_USBtoUART \
  app-flash monitor
```

## CLion

Открывайте в CLion именно папку:

```text
ports/espidf
```

Общие пресеты лежат в `CMakePresets.json`. Локальные пути к ESP-IDF,
Python venv, toolchain и serial port задавайте в `CMakeUserPresets.json`.
Этот файл намеренно игнорируется Git.

Минимальный локальный preset должен наследоваться от:

```text
debug-m5stack-basic-v1
```

и задавать:

- `IDF_PATH`;
- `IDF_TOOLS_PATH`;
- `ESPPORT`;
- `ESPBAUD`;
- `PATH` с ESP-IDF Python venv и Xtensa toolchain;
- `PYTHON`;
- `CMAKE_C_COMPILER`;
- `CMAKE_CXX_COMPILER`;
- при необходимости `CMAKE_MAKE_PROGRAM`.

## Smoke-test после прошивки

После загрузки shell выполните:

```text
version
board
esp32-info
uptime
tasks
mem
help
pwd
fs-info
tmp-info
echo hello
touch /tmp/a.txt
write /tmp/a.txt hello
cat /tmp/a.txt
append /tmp/a.txt world
cat /tmp/a.txt
rm /tmp/a.txt
devices
input-test
keyboard-live -s 10 m5stack-buttons
keyboard-raw -s 10 m5stack-buttons
esp32-spi-test
display-info
display-fill red
display-fill green
display-test
```

После включения SD/FatFs дополнительно проверьте:

```text
storage
block-devs
ls /mnt/sd
fs-smoke /mnt/sd
```

Для Cardputer-Adv минимальный bring-up smoke:

```text
board
devices
display-info
display-test
storage
block-devs
fs-info
mount
power
console-info
keyboard-live -s 2 -i 100
keyboard-raw -s 2 -i 100
gamepad-live -s 2 -i 100
imu -n 8 -i 100
ir info
ir samsung-power -r 1
audio
beep --frequency 880 --duration 80 --volume 96
mplay --test
mic info
mic sample
mic loopback -s 1 -g 2
gb-info /mnt/sd/gb/test-rom-2.gb
gbemu --profile --frames 60 --display none /mnt/sd/gb/test-rom-2.gb
gbemu --audio --profile --frames 600 --display none /mnt/sd/gb/test-rom-2.gb
clear
```

`power` и статус-бар на Cardputer-Adv показывают оценку уровня по ADC-напряжению. Аппаратный статус зарядки и ток
недоступны, поэтому ожидаемый вывод содержит `charging=unavailable`, а charging icon в статус-баре не должен
появляться.

`keyboard-live` и `keyboard-raw` в простое должны завершаться без событий. Если при ручном наборе снова появляются
лишние символы, сначала проверяйте raw-координаты: драйвер игнорирует неиспользуемые TCA8418 columns/rows и сбрасывает
локальное состояние после FIFO overflow, поэтому повторяемый мусор должен быть виден как реальное raw-событие.

`audio` должен показывать `cardputer-adv-audio`, `tone: yes`, `mix: no`, `pcm: yes`, а `beep` должен завершаться
строкой `beep: tone ...`. `mplay --test` проверяет PCM path и должен завершаться без underruns. Cardputer-Adv
использует ES8311 codec на I2C `0x18` и I2S1 speaker output; headphone jack может отключать встроенный усилитель.

`mic` должен показывать `cardputer-adv-mic`, `pcm-capture: yes`, `sample-rate-hz: 44100`; `mic sample` печатает
peak/average, а `mic loopback -s 1 -g 2` должен вернуть `loopback frames=...` без `task-create-failed`.

`imu` проверяет BMI270 на I2C `0x69`. `ir samsung-power -r 1` проверяет только отправку RMT-кадра на GPIO44; реакция
Samsung TV остаётся ручным line-of-sight тестом. Для других Samsung TV-кодов можно использовать `ir samsung-code -c`.

`gbemu` в локальном Cardputer-Adv preset включён с dynamic compact ROM cache: ожидаемая строка запуска содержит
`rom-cache=8Kx6=48K`. В playable-режиме Cardputer-Adv автоматически использует mapped flash staging в ESP-IDF
`storage` partition (`0x300000`, 1 MiB), чтобы убрать SD/FAT cache misses из hot path. Быстрый регрессионный запуск
`gbemu --profile --frames 60 --display none /mnt/sd/gb/test-rom-2.gb` должен печатать `rom-storage=flash`,
`cache-fill=0` и возвращаться в shell без `task-create-failed`. Для проверки Game Boy PCM используйте
`gbemu --audio --profile --frames 600 --display none /mnt/sd/gb/test-rom-2.gb`: в summary должны быть `audio=1` и
ненулевые `audio frames queued/written`. Launcher `gbemu` без аргументов читает `[audio] enabled`; на Cardputer-Adv
отсутствующий ключ трактуется как `true`, поэтому запуск ROM из меню передаёт `--audio`.

`clear` проверяется визуально: после команды shared console clear path очищает всю область текста ниже status bar,
включая пиксели за пределами последней логической колонки/строки.

## Кнопки M5Stack Basic

После включения input backend регистрируется устройство:

```text
m5stack-buttons
```

Кнопки читаются как `a`, `b`, `c` и доступны через `input-test`,
`keyboard-live` и `keyboard-raw`.

## SPI backend

Команда `esp32-spi-test` инициализирует общую SPI-шину M5Stack Basic
по board descriptor и отправляет 10 idle-байт `0xFF` при неактивном CS.
Она не монтирует TF-карту и не проверяет файловую систему, а только
подтверждает, что ESP-IDF SPI bus/device path готов для следующих
патчей LCD и SD.

## Экран M5Stack Basic

После включения display backend регистрируется устройство:

```text
m5stack-lcd
```

Экран подключён к общей SPI-шине LCD/TF и использует отдельные `CS=14`, `DC=27`, `RST=33`, `BL=32`.
