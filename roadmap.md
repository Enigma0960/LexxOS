# Roadmap LexxOS

Единый список активных задач. Состав 0.1.0, известные ограничения и финальные
действия перед публикацией находятся в [описании выпуска](docs/release-0.1.0.md).
[Результаты проверок](docs/tests/release-0.1.0-validation.md) отделяют проверку
сборки от аппаратной. Выпуск ещё не опубликован.

## Приоритеты

- Зафиксировать кандидат, повторить его локальные проверки и собрать чистые архивы.
- Длительные аппаратные сценарии оставлены для ручного запуска автором.
- Накапливать статистику единичного reset и остаточного UART-ввода JC;
  наблюдения приняты как неблокирующие, первопричина пока не установлена.
- Внешнюю периферию проверять отдельно; экспериментальные системы не включать
  в обязательный базовый регресс. CI отложен и не блокирует 0.1.0.
- Рефакторинги ниже выполнять небольшими проверяемыми изменениями; сами по себе
  они не являются обязательным объёмом первого выпуска.

Архитектурные обоснования: [архитектурный план](docs/architecture-roadmap.md).
Исторические результаты 0.0.7 не подтверждают аппаратную работу кандидата 0.1.0.

## Рефакторинг и единообразие

- Выделить DNS/ping/NTP из `pico_network_support.cpp`, сохранив общую инициализацию
  CYW43, flags и CLI.
- Разделить модель захвата, storage и UI `gpio_analyzer_program.cpp`, сохранив
  память, тайминги и формат данных.
- Перед удалением legacy/compatibility API доказать отсутствие callers во всех
  профилях; действующие aliases и экспериментальные драйверы сохранять.
- Аппаратные справочники команд сверять с UART help dump соответствующего профиля.
- Выделить внутренние trace/profile-возможности `gbemu` из пользовательского приложения;
  расширять каталог возможностей вместе с board providers; измерять flash/RAM.

Большие активные модули не удаляются только из-за размера. Форматирование — отдельный
механический патч по [code-style](docs/code-style.md), без смешивания с поведением.
Реализованные группы команд и их условия описаны в [матрице](docs/command-matrix.md).

## Архитектурный backlog после стабилизации релиза

- Разделить board identity и component declarations; развивать driver providers так,
  чтобы новый драйвер не требовал изменений core DeviceManager.
- Ввести ProgramModule и регистрацию модулей в target composition; сохранить генерацию help/docs.
- Завершать миграцию global adapters по конкретным потребителям; отдельно проверить FatFs volume-slot ownership,
  если понадобятся независимые FAT pools.
- Выделить debounce policy и добавить isolated hot-plug test с fake clock/block device.
- Специализированные runtime profiles развивать поверх реализованной minimal/full матрицы.
- Довести lifecycle/ownership и арбитраж устройств перед настоящим параллельным запуском программ.
- После устранения глобального состояния исследовать host plugins, versioned external-program API
  и capability-based ограничения. Не включать это в обязательный объём 0.1.0.
- FAT repair-mode — отдельная будущая функция; read-only fsck не считать незавершённой записью FAT.

## Перенесённые задачи подсистем

Следующие пункты перенесены из прежнего roadmap. Их статус требует сверки с текущим кодом
и свежими проверками; они не становятся автоматически блокерами 0.1.0.

## PicoCalc

### PicoCalc Pico W

- PSRAM: после 8 MHz cap для первого Pico W повторить `psram-info`/`psram-test` на реальном RP2040. После
  подтверждённого data path выбрать назначение: распределитель памяти, пул framebuffer/cache или блочное устройство.
  Отдельно спроектировать более быстрый quad/PIO backend, если он понадобится.
- Audio: `mplayer` отключён по умолчанию через `LEXXOS_FEATURE_MIDI`; перед включением в standard проверить
  `mplayer /mnt/sd/midi/example.mid` на реальном PicoCalc, настроить громкость/тембры по фактическому звуку и решить,
  нужен ли speaker/headphone routing. Следующий backend после software PWM: DMA/PIO/I2S или другой неблокирующий путь.
- Wi-Fi/RTC: hardware smoke подтвердил `wifi-config`/boot auto-connect, `wifi-status`, DNS/ICMP и
  `rtc-sync --tz +07:00`.
  После релиза решить retry/roaming policy и хранение credentials не в plain text.
- Bluetooth: держать ниже Wi-Fi до появления практического use case.
- JCS software `poweroff`: Pico 2 W smoke 2026-07-08 подтвердил sync, unmount и фактическое выключение; старый Pico W
  smoke видел `southbridge request failed`, поэтому release notes должны назвать поведение board/JCS-dependent.
- Pico W PSRAM cap retest: прошить новый `picocalc_pico_w` UF2 на реальный RP2040 PicoCalc и повторить `board`,
  `psram-info`, `psram-test`, `display-test --bench --frames 1 --transport all --size gb1 --rows 16`, SD/FAT, Wi-Fi и
  reboot path.
- Console prompt redraw retest: на новом UF2 набрать часть строки до delayed `wifi-autoconnect`/`FreeRTOS task start`
  output и подтвердить, что сообщение вставляется выше prompt, а введённый текст сохраняется и виден без Tab redraw.
- UART paste / long LFN completion retest: на новом UF2 быстро вставить длинную строку через UART и проверить Tab
  completion для `/mnt/sd/midi/ExampleMidiFile_T...` и `/mnt/sd/midi/ExampleMidiFile_F...`.
- RP2040 heavy features: перед любым возвратом Game Boy, MIDI `mplayer` или GPIO analyzer в первый Pico W профиль
  вынести дополнительные рабочие области в `ProgramMemoryArena`/PSRAM-aware backend и проверять map-файл на запас между
  `.heap` и `.stack1_dummy`. `LEXXOS_ALLOW_PICOCALC_PICO_W_HEAVY_FEATURES=ON` держать только экспериментальным override.
- Southbridge snapshot hardening: одиночный `update-keyboard-fifo-read` warning оформлен как documented known issue.
  Если будущие smoke покажут серии ошибок, потерю ввода или `snapshot-ok=no`, добавить retry/backoff для plain
  diagnostic path.
- Политика низкого заряда: сейчас есть отображение battery/status; активное предупреждение или автосон ещё не
  реализованы.

### PicoCalc Pico 2 W

- После подтверждённого smoke 2026-07-08 держать Pico 2 W в regression checklist вместе с Pico W перед крупными
  изменениями LCD/southbridge/SD/Wi-Fi path.

### Keyboard And Hotkeys

- Назначать `F1..F10` только под конкретные сценарии shell/editor/help.
- Уточнить семантику `Insert`, если появится insert/overwrite mode в editor.
- Расширять Alt/Sym layer только по фактическим scan-code и UI-сценариям.
- Для каждой новой горячей клавиши добавить проверку до включения в основную раскладку.

## Приложения и команды

- Ввести `ProgramModule`: новый набор команд должен добавляться одним module source list и одной записью target
  composition, а не ручными правками центрального регистратора.
- Сохранить генерацию `docs/commands/` из descriptors/runtime help после будущей модульной регистрации.
- Добавить contract tests для важных команд с состоянием: `display-power timeout`, загрузка/сохранение конфигурации
  backlight и service-backed пути PicoCalc power/backlight.
- Развивать screen-oriented app contract поверх `terminal::readKey()` только по реальным сценариям: editor, help UI,
  будущие network/audio tools.

## Архитектура и runtime

- Подготовить будущий `ProgramRunner`: shell остаётся supervisor-ом, программа получает отдельную task, Ctrl+C идёт
  через `CancellationToken`, stdout/stderr остаются stream contracts.
- Довести экспериментальный `ProgramSupervisor`: сейчас есть `run`/`jobs`/`stop`, foreground/background task path,
  stack high-water mark, cooperative cancellation, локальный process table guard и per-process `ProgramMemoryArena`;
  следующий шаг — переносимый kernel lock, resource ownership и console ownership для настоящих параллельных запусков.
- Подготовить PSRAM-aware runtime path: `ProgramRuntimeProfile::memoryClass` и `externalMemoryQuotaBytes` уже проходят
  через descriptors/snapshots/`jobs`, но arena пока только проверяет policy и ещё не выдаёт реальные PSRAM allocations.

## Профили сборки и платы

- STM32: RWX warning устранён read-only таблицами конструкторов во flash; повторить boot на плате.

## Storage и файловые системы

- Python emulator `--sd-dir`: FAT32 write-back реализован явной командой `sync` с проверкой конфликтов.
  Дальше проверить восстановление после ошибки записи и согласованность при фоновых задачах; FAT16 остаётся import-only.
- CMD25 и RP2040 SDIO skeleton держать как guarded experimental tasks, отдельно от production SPI/FAT path.

## Game Boy: дальнейшее развитие

Задачи перенесены из прежнего Game Boy backlog; перед реализацией сверять с текущим core,
в том числе уже существующим `src/lexxos/gameboy/apu.cpp`.

### Следующий milestone

Цель следующего крупного этапа: довести первую экранную версию `gbemu` до удобного игрового flow: выход из игры,
сохранения, SRAM/save states и полировку settings UI.

Готово в первой TUI-версии:

- `gbemu` без параметров открывает ROM launcher;
- `gbemu <rom>` из shell запускается напрямую как сейчас;
- ROM launcher показывает `.gb` и `.gbc` файлы из `rom_dir`;
- settings menu редактирует display, palette, render/input throttle, realtime sync, speed/ROM storage modes,
  fast timing/halt и debug flags;
- настройки и `last_rom` сохраняются на SD;
- после bounded/diagnostic выхода из запущенного ROM launcher возвращается в меню.

Критерии следующей готовности:

- Start+Select или меню подтверждения штатно завершает игру во время длинного запуска;
- shell cancellation корректно возвращает управление без reset платы;
- playable profile для Zelda/Pokémon не регрессирует;
- settings UI умеет менять `rom_dir`, а не только читать его из файла.

### Settings

Путь:

```text
/mnt/sd/apps/gbemu/settings.ini
```

Начальный формат:

```ini
[general]
rom_dir = /mnt/sd/gb
last_rom =
profile = playable

[video]
display = crop
render_every = 4
sync_realtime = true

[palette]
name = dmg_green

[input]
input_every = 2
exit_combo = start+select
exit_hold_ms = 1000

[performance]
rom_flash_reuse = false
fast_timing = true
fast_halt = true

[experimental]
speed_mode = playable
frames = 16384
steps_per_frame = 512
cycles_per_frame = 0
warmup = off
max_cycles = 8988672
min_tile_data = 256
rom_storage = auto
no_ppu = false

[debug]
profile = false
profile_every = 120
fault_trace = false
```

`frames = 0` теперь означает запуск без ограничения числа кадров.

Осталось:

- добавить редактирование `rom_dir` из UI;
- добавить preview palette/settings перед запуском.

### ROM launcher

Минимальный экран:

```text
LexxOS Game Boy

> Zelda.gb
  Pokemon.gb
  Tetris.gb

A / OK      run
B / Back    back
Start       settings
Select      exit
```

Осталось:

- добавить refresh/rescan command;
- добавить подтверждение выхода, когда игра уже запускалась;
- улучшить текст ошибок для missing SD/missing `rom_dir`.

### Exit and cancellation

Осталось:

- добавить small menu `Resume / Reset / Save / Load / Exit`;
- при запуске из меню возвращаться в menu;
- при запуске из shell возвращаться в shell;
- убедиться, что cooperative cancellation через `CancellationToken` сохраняет display/input state и возвращает prompt.

### Saves

SRAM save files:

```text
/mnt/sd/apps/gbemu/saves/<rom-name>.sav
```

Осталось:

- реализовать external RAM persistence для MBC1/MBC3/MBC5;
- грузить `.sav` при запуске ROM;
- сохранять при выходе, периодически по dirty flag и вручную из меню;
- привязать save к ROM checksum/header, чтобы одинаковые имена файлов не конфликтовали.

Save states:

```text
/mnt/sd/apps/gbemu/states/<rom-name>.slot0.state
```

Осталось:

- описать полный serializable state CPU/timer/PPU/VRAM/OAM/WRAM/HRAM/IO/mapper;
- сделать один slot;
- добавить save/load из меню;
- затем расширить до нескольких slots.

### Palettes

Осталось:

- добавить preview в меню настроек.

### Audio

Game Boy APU не блокирует ROM launcher/saves milestone.

Осталось:

- спроектировать APU channels: square 1, square 2, wave, noise, mixer;
- выбрать output backend для PicoCalc/других плат;
- добавить settings `audio on/off`, `volume`, `sample_rate`;
- проверить CPU cost до включения по умолчанию.

### Performance

Осталось после UI/saves:

- рассмотреть async/DMA blit;
- оптимизировать CPU hot path и PPU tile fetch;
- улучшить стабильность `render-every=3`;
- проверить частичный redraw только изменённых зон.

`gpio-analyzer` признан экспериментальным: поставляется только в full на поддерживаемых платах; minimal/standard
принудительно отключают его, включая старый cache override.
