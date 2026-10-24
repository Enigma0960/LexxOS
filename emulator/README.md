# LexxOS Python Emulator

Этот каталог содержит первый вариант Python-эмулятора LexxOS.

Текущая модель намеренно сделана на уровне, полезном для отладки бизнес-логики ОС:

- LexxOS собирается как shared library `lexxos_sim`.
- Python загружает библиотеку через `ctypes`.
- Консоль подключена через виртуальный UART0.
- `/dev/sd0` подключается как block-level устройство, читающее raw/FAT image из Python или временный FAT16-образ,
  собранный из локальной папки.
- SD card detect смоделирован через GP22: активный уровень — low.
- I2C debug bus поддерживает `probe/write` через Python-объекты.
- `emu_display` подключается как RGB565 framebuffer. Размер экрана задаётся при старте эмулятора.
- `emu_pwm_audio` / `emu_adc_audio` подключаются как primary `AudioDevice`; tone и PCM могут проигрываться на host, а
  сами запросы остаются доступны в Python sink/statistics.
- Планировщик на первом этапе использует host threads, поэтому это ещё не полностью детерминированная симуляция времени.

## Сборка и быстрый запуск GUI

Установите CMake 3.25+, Ninja, C/C++20 compiler и GUI-зависимости:

```bash
uv sync --project emulator --locked
uv run --project emulator python emulator/lexxos_emu/gui.py --profile minimal
# Все поддерживаемые системы, включая Game Boy и MIDI:
uv run --project emulator python emulator/lexxos_emu/gui.py --profile full
```

GUI перед запуском конфигурирует публичный preset, выполняет инкрементальную сборку
`lexxos_sim`, загружает полученную библиотеку и автоматически делает Boot.
В панели **Audio** можно выбрать эмулируемое устройство (PWM/ADC/Off) и вывод на компьютер
(Auto/On/Off/WAV). В GUI по умолчанию выбраны PWM + Auto: если установлен Python-модуль
`sounddevice` и работает аудиоустройство ОС, `beep` и внешние игры воспроизводят звук.
Параметры можно задать при запуске: `--audio-mode pwm --audio-output on`.
Для диагностики отсутствия динамиков в качестве вывода выберите `WAV file` и задайте путь.
+Статус работы звука отображается в той же панели. Настройки применяются при следующем Boot/Reset.
Ручная сборка dylib не требуется. По умолчанию выбран minimal: shell, VFS, SD/FatFs,
экран и ввод сохранены; Game Boy, MIDI, GPIO analyzer и расширенная диагностика выключены.
Full исключает legacy FAT и legacy debug/status task.

`--build-only` собирает без Qt и окна; `--no-build` использует уже собранный выбранный
профиль. `--lib /path/to/library` явно отключает автоматическую сборку. Ошибка сборки
останавливает запуск, старая библиотека вместо новой не загружается.
Для нестандартного пути к CMake используется `LEXXOS_CMAKE_BIN`.

```bash
uv run --project emulator python emulator/lexxos_emu/gui.py --profile full --build-only
# Эквивалентная ручная сборка:
cmake --preset full-dev-sim
cmake --build --preset full-dev-sim
```

Библиотеки находятся в `build/<minimal|full>-dev-sim/src/`:
`liblexxos_sim.dylib` (macOS), `liblexxos_sim.so` (Linux), `lexxos_sim.dll` (Windows).
Старый `debug-sim` остаётся доступен, но для новых запусков используйте публичные профили.

## Запуск без SD-карты

```bash
PYTHONPATH=emulator python -m lexxos_emu.run \
  --lib build/full-dev-sim/src/liblexxos_sim.dylib
```

Размер framebuffer можно задать при старте:

```bash
PYTHONPATH=emulator python -m lexxos_emu.run \
  --lib build/full-dev-sim/src/liblexxos_sim.dylib \
  --display-size 480x272
```

Консольный запуск может открыть отдельное окно `emu_display`, когда LexxOS впервые нарисует кадр. По умолчанию это
делается автоматически только в интерактивном TTY и при доступной GUI-сессии; режим можно переопределить:

```bash
PYTHONPATH=emulator python -m lexxos_emu.run \
  --lib build/full-dev-sim/src/liblexxos_sim.dylib \
  --display-window on
```

Аудио-эмуляция выбирается через `--audio-mode off|pwm|adc`; по умолчанию включён PWM:

```bash
PYTHONPATH=emulator python -m lexxos_emu.run \
  --lib build/full-dev-sim/src/liblexxos_sim.dylib \
  --audio-mode adc
```

Tone-запросы и PCM-поток можно выводить в host-аудио через `--audio-output auto|on|off|wav`. В `auto` эмулятор
пробует Python backend `sounddevice`. Если backend недоступен, он молча продолжает без звука; `on` считает отсутствие
backend ошибкой запуска. `sounddevice` дает непрерывный realtime PCM stream для `mplay`, а `wav` пишет один чистый файл
без захвата через микрофон.

```bash
PYTHONPATH=emulator python -m lexxos_emu.run \
  --lib build/full-dev-sim/src/liblexxos_sim.dylib \
  --audio-output on
```

```bash
PYTHONPATH=emulator python -m lexxos_emu.run \
  --lib build/full-dev-sim/src/liblexxos_sim.dylib \
  --audio-output wav \
  --audio-wav-path temp/temp/mplay.wav
```

## Запуск с образом SD-карты

```bash
PYTHONPATH=emulator python -m lexxos_emu.run \
  --lib build/full-dev-sim/src/liblexxos_sim.dylib \
  --sd-image /path/to/sd.img
```

## Запуск с локальной папкой вместо SD-образа

Папку можно импортировать как содержимое `/dev/sd0`. При старте эмулятор создаёт временный FAT16-образ из файлов и
каталогов этой папки, а LexxOS видит его как обычную SD-карту.

```bash
PYTHONPATH=emulator python -m lexxos_emu.run \
  --lib build/full-dev-sim/src/liblexxos_sim.dylib \
  --sd-dir /path/to/host/folder
```

В режиме `--sd-dir --sd-fs fat32` команда `sync` сначала выполняет обычный `vfs::sync()`, а затем
сверяет SHA-256 файлов на хосте и в FAT32 со снимком предыдущей синхронизации.
Если логических изменений нет, FAT-том **не размонтируется**. При изменениях том временно
размонтируется, обновляется sparse FAT32-образ и выполняется повторное монтирование.
Новые файлы на компьютере становятся видны в `/mnt/sd` после команды `sync` без перезапуска GUI.
Изменения LexxOS передаются обратно в исходную папку только при успешном разрешении конфликтов.
Если одинаковый файл по-разному изменён на обеих сторонах, `sync` откажется перезаписывать его.

Предостережение: обновление образа предполагает отсутствие параллельного обращения к SD со
стороны фоновых задач во время синхронизации. Для тестов используйте `sync` после завершения
программ, которые держат открытые файлы; не редактируйте host-файлы одновременно с `sync`.
В режиме FAT16 и для `--sd-image` поведение `sync` остаётся прежним (только VFS flush).

После запуска можно вводить команды shell, например:

```text
help
storage
ls /
ls /mnt/sd
cat /mnt/sd/README.TXT
exit
```

Интерактивный консольный запуск переводит настоящий terminal/TTY в raw-режим. Это нужно, чтобы стрелки, Delete и другие
управляющие клавиши доходили до LexxOS как байты UART, а не обрабатывались host-терминалом. Если запускать `run.py` из
IDE-консоли без настоящего TTY, ввод будет работать в построчном fallback-режиме, но live-перемещение курсора до нажатия
Enter будет недоступно. Для проверки редактора строки используйте macOS Terminal, встроенный Terminal в CLion или другой
PTY-совместимый терминал.

Когда окно `emu_display` открыто и находится в фокусе, обычные клавиши отправляются в нативный `sim-keyboard` input
device. Для сочетаний, которые часто перехватывает host-система или терминал, в окне есть кнопки `Alt+Tab` и `Ctrl+C`;
они отправляют события с соответствующими модификаторами напрямую в LexxOS.

## Минимальный Python-сценарий

```python
from lexxos_emu import LexxOsSim, VirtualBoard

board = VirtualBoard.dev_pico(sd_image="sd.img", sd_inserted=True)
with LexxOsSim("build/full-dev-sim/src/liblexxos_sim.dylib", board) as sim:
    sim.set_sd_inserted(True)
    sim.boot()
    print(sim.run_until_output_contains("lexx:/>"))
    sim.write_line("help")
    print(sim.run_until_idle())
```

## Запуск графического эмулятора PySide6

Графический интерфейс находится в модуле `lexxos_emu.gui`. Он использует тот же `LexxOsSim` и `VirtualBoard`, что и
консольный запуск, но добавляет:

- отдельное окно с консолью LexxOS;
- управление `Boot`, `Shutdown`, `Reset`;
- переключатель SD card detect через GP22;
- ручные GPIO-входы GP2-GP5 для будущих драйверных тестов;
- управление I2C-устройствами на debug bus и кнопку `i2c-scan --quiet`.

Установка GUI-зависимости:

```bash
uv sync --project emulator --locked
```

Запуск:

```bash
uv run --project emulator python emulator/lexxos_emu/gui.py --profile full \
  --oled
```

С образом SD-карты:

```bash
uv run --project emulator python emulator/lexxos_emu/gui.py --profile full \
  --sd-image /path/to/sd.img \
  --oled
```

С локальной папкой вместо SD-образа:

```bash
uv run --project emulator python emulator/lexxos_emu/gui.py --profile full \
  --sd-dir /path/to/host/folder \
  --oled
```

В GUI этот режим выбирается в `SD source` → `Host directory`.

Для текущего этапа I2C-эмуляция сделана как ответ на `probe`: если адрес добавлен в GUI, команда `i2c-scan` увидит
устройство. Полная эмуляция протокола устройства может быть добавлена позднее поверх текущей модели `VirtualI2CBus`.

## Текущие ограничения

Это стартовая версия для запуска кода в эмуляторе, а не финальная аппаратная симуляция RP2040.

Ограничения:

- SD эмулируется на уровне block device, не на уровне SPI-протокола.
- Audio backend выводит host-звук только когда доступен `sounddevice` или внешний helper. Лучший путь для PCM —
  `sounddevice`; raw subprocess stream и WAV playback остаются fallback.
- Режим `--sd-dir` по умолчанию создаёт временный FAT32-образ. Двусторонний перенос
  изменений выполняется явной командой `sync` с проверкой конфликтов; автоматической
  фоновой синхронизации нет. В FAT16 доступен только первоначальный импорт.
- Scheduler использует host threads.
- В одном Python-процессе следует запускать один экземпляр LexxOS simulator.
- Виртуальное время пока используется для `uptime`, но задержки задач всё ещё выполняются через host wait.

Следующий логичный этап — заменить `scheduler_sim.cpp` на кооперативный event/tick scheduler и сделать `run_ms()`
полностью виртуальным.

## Терминал в GUI (ANSI/VT100)

Правая панель принимает клавиши непосредственно, без отдельной строки и кнопки Send.
Ввод идёт в UART RX как последовательность байтов. Сама LexxOS отвечает за
эхо, редактирование строки, историю команд и обработку Ctrl+C.

Поддерживаются: CR, LF, BS, TAB, ESC [ A/B/C/D, ESC [ H/f, ESC [ G,
ESC [ J/K, ESC [ m (16/256/truecolor), ESC [ s/u, ESC 7/8,
ESC [ ? 25 h/l и альтернативный экран ?1049h/l.

Enter отправляет CR, Backspace — DEL, клавиши навигации — CSI.
Ctrl+C посылает ETX (0x03), Ctrl+Shift+C копирует видимый экран,
Cmd+V/Ctrl+V вставляет текст из буфера обмена (максимум 8 KiB за вставку).
Колёсико прокручивает историю вывода. Команды shell и диагностические кнопки
отправляются программно; их эхо выполняет только сама LexxOS.

Терминал реализует полезное подмножество VT100, не является полной xterm
эмуляцией (нет DEC-атрибутов, мыши и полноценного Unicode cell width/reflow).

## SD FAT32 (Release/0.1.0)

Для `--sd-dir` по умолчанию используется разреженный FAT32-образ на 16 ГиБ.
Размер образа — логический: место на хосте расходуется преимущественно под импортированные файлы и FAT.
Копирование потоковое; образ создаётся во временном каталоге и удаляется после завершения эмулятора.

```bash
uv run --project emulator python emulator/lexxos_emu/gui.py --profile full --sd-dir ./temp --sd-capacity 16G --sd-fs fat32
# Также доступны 256M, 512M и 32G; 16/32 ГиБ используют FAT32.
uv run --project emulator python emulator/lexxos_emu/gui.py --profile full --sd-image ./card.img --sd-writable
```

**Важно:** `--sd-image` по умолчанию открывается только для чтения. С `--sd-writable`
запись сохраняется непосредственно в образе. Сохраните резервную копию перед использованием.

Чтобы экспортировать изменения из временной SD-карты, корректно завершите работу LexxOS,
затем нажмите «Export SD snapshot to new folder…» в GUI. Экспорт создаёт **новую** папку,
не заменяет и не удаляет файлы исходного `--sd-dir`.
Консольный эмулятор поддерживает `--sd-export-to /path/to/new/folder` (после остановки ОС).
Это безопасный экспорт полного снимка, а не двусторонняя синхронизация с разрешением конфликтов.
В режиме FAT16 можно выбрать только 256M/512M; исторический FAT16-генератор продолжает
использовать буферизацию в памяти, поэтому для больших данных предпочитайте FAT32.

Проверка Python-генератора без сборки C++:

```bash
PYTHONPATH=emulator python -m pytest emulator/tests/test_fat32_image.py
```
