# LexxOS Game Boy emulator

Документ фиксирует текущее состояние `gbemu` как основную документацию. Живой backlog следующего этапа находится в
[`docs/todo/gameboy.md`](todo/gameboy.md).

## Статус

`gbemu` является рабочим прототипом Game Boy emulator для LexxOS на RP2040/full builds и на Cardputer-Adv ESP-IDF
bring-up preset. В standard PicoCalc release Game Boy отключён, потому что RP2040/PicoCalc без внешней памяти не
держит полный emulator workspace; Cardputer-Adv использует отдельный compact heap profile без PSRAM.

Уже реализовано:

- запуск ROM с SD-карты через VFS/FatFs;
- TUI ROM launcher по `gbemu` без аргументов;
- reusable TUI framework для простых экранных меню;
- MBC1 для Zelda-class ROM и MBC5 для Pokémon-class ROM;
- загрузка ROM в RP2040 XIP flash через `--rom-flash`;
- переиспользование ROM во flash через `--rom-flash-reuse`;
- ESP-IDF/Cardputer-Adv mapped flash staging через `storage` partition;
- быстрый playback path: `--fast-timing`, corrected STAT polling и `fast-halt`;
- вывод на RGB565 display в режимах `crop`, `fit` и `none`;
- built-in RGB565 palettes через `--palette`;
- frameskip через `--render-every`;
- input polling throttle через `--input-every`;
- playback preset `--preset playable`;
- advanced/experimental launcher settings: `speed_mode`, frame/step/cycle limits, warmup mode, ROM storage backend и
  `no_ppu`;
- settings file `/mnt/sd/apps/gbemu/settings.ini` как runtime defaults и хранилище launcher settings;
- settings menu для display, palette, render/input throttle, realtime sync, audio, speed modes, ROM storage,
  fast timing/halt и debug flags;
- realtime limiter через `--sync-realtime`;
- bounded runs через `--frames`, unlimited runs через `frames = 0`, `--cycles-per-frame`, `--max-cycles`;
- Start+Select exit combo during emulation;
- опциональный Game Boy APU PCM output через `--audio`; прямой ESP-IDF/Cardputer-Adv shell-запуск без `--audio`
  остаётся тихим, а TUI launcher включает звук через `[audio] enabled = true`;
- diagnostic flags: `--profile`, `--fault-trace`, `--watch-blank`, `--break-pc`, `--break-pc-from`,
  `--break-bgp-zero-after`;
- `gbemu-config` для создания/просмотра settings file;
- command docs и emulator tests для Game Boy diagnostic commands;
- Cardputer-Adv keyboard-backed `GamepadDevice` с раскладкой `e/z/a/d`, `[`/`]`, `;`, `` ` ``, Enter и Backspace;
- ESP-IDF/Cardputer-Adv dynamic compact ROM cache `8Kx6=48K` и default mapped-flash playback path.

Найденная ранее проблема зависания Zelda после первого нажатия закрыта: причина была в неточном `STAT` behavior при
`--fast-timing`, а не в неизвестной CPU-инструкции.

## Playable profile

Рекомендуемый текущий запуск:

```text
gbemu /mnt/sd/gb/test-rom-2.gb --frames 16384 --preset playable --display crop
```

Стабильные настройки:

```text
display=crop
palette=dmg_green или default
real-frame=on
render-every=4
input-every=2
rom-flash-reuse=on по умолчанию на ESP-IDF/Cardputer-Adv, on вручную для Pico/RP2040 flash staging
fast-timing=on
fast-halt=on
sync-realtime=on
```

В settings file тот же режим задаётся как `[experimental] speed_mode = playable`. Для текущего запуска можно явно
передать `--preset playable` или выбрать быстрые shortcut-режимы `--playable-fast`, `--max-speed`, `--psram-fast`.
Явно переданные `--frames`, `--steps-per-frame`, `--cycles-per-frame`, `--render-every`, `--input-every`,
`--rom-flash`/`--rom-flash-reuse`/`--rom-psram`/`--rom-psram-reuse` имеют приоритет над settings defaults.

`render-every=3` остаётся экспериментальным: картинка плавнее, но производительность не всегда держит realtime на
текущем display/CPU path.

## Settings

`gbemu-config --init`, первый прямой запуск `gbemu <rom>` и первый запуск launcher используют общий defaults-шаблон:

```text
/mnt/sd/apps/gbemu/settings.ini
```

Если файл есть, `gbemu` читает его как runtime defaults. CLI-аргументы имеют приоритет над settings file. Если файла
нет, `gbemu` best-effort создаёт defaults на SD; если `/mnt/sd` недоступен, команда тихо использует built-in defaults.
Если файл повреждён, слишком большой или содержит некорректное значение, `gbemu` печатает warning и продолжает с
built-in defaults.

Launcher читает `[general] rom_dir`, по умолчанию `/mnt/sd/gb`, показывает `.gb`/`.gbc` файлы, сохраняет выбранный
`last_rom` и применяет settings перед запуском выбранного ROM.

TUI controls:

```text
Up/Down     select ROM or setting
A / Enter   run ROM or edit selected setting
Left/Right  change setting value
Start       settings
B / Escape  back or exit ROM list
Select / Backspace
            exit ROM list
```

В TUI settings блок `Advanced / experimental` редактирует:

```ini
[audio]
enabled = true

[experimental]
speed_mode = playable        # playable, playable-fast, max-speed, psram-fast, manual
frames = 16384
steps_per_frame = 512
cycles_per_frame = 0
warmup = off                 # off, vram, visible, tile-data, bg-ready
max_cycles = 8988672
min_tile_data = 256
rom_storage = auto           # auto, file-cache, flash, flash-reuse, psram, psram-reuse
no_ppu = false
```

`frames = 0` запускает ROM без ограничения числа кадров; выйти можно через Start+Select hold
(`Tab+Backspace` на клавиатурном управлении), cancel или ошибку
эмуляции. Для максимальной скорости на Pico/Pico 2 W лучше тестировать `rom_storage = flash-reuse`, чтобы убрать
FAT/SD cache misses из hot path; `psram-reuse` безопаснее для flash, но обычно медленнее. На Cardputer-Adv playable
режим автоматически использует mapped flash staging даже если старый settings-файл содержит `rom_storage = file-cache`.

Для Pico 2 W builds с `LEXXOS_FEATURE_GAMEBOY=ON` и пустым `LEXXOS_PICO_SYS_CLOCK_KHZ` CMake автоматически задаёт
`LEXXOS_PICO_SYS_CLOCK_KHZ=250000` до старта FreeRTOS. Двухъядерные Pico/Pico 2 builds могут закреплять `gbemu` за
core1 и SD worker за core0; для RP2350 двухъядерный preset использует SMP FreeRTOS port.
Явное значение cache-переменной имеет приоритет только в безопасном диапазоне. Для Wi-Fi-enabled Pico boards
(`dev_pico2w`, `picocalc_pico_w`, `picocalc_pico2w`) CMake запрещает `LEXXOS_PICO_SYS_CLOCK_KHZ` выше `250000`:
на PicoCalc Pico 2 W SMP/Game Boy smoke 2026-07-11 режим `266000` ломал CYW43 path (`wifi-connect` оставался в
`join`, `wifi-scan` таймаутился), а `250000` подтвердил boot `wifi-autoconnect`, `wifi-status`, `wifi-scan` и `ping`.

Для Cardputer-Adv ESP-IDF preset `user-espidf-m5stack_cardputer` CMake выставляет
`LEXXOS_GAMEBOY_ROM_CACHE_BANKS=3`, `LEXXOS_GAMEBOY_ROM_CACHE_PAGE_BYTES=8192` и
`LEXXOS_GAMEBOY_DYNAMIC_ROM_CACHE=1`. Важно, что эти значения должны быть переданы именно в ESP-IDF component compile
definitions; иначе runtime возвращается к default cache и workspace/cache не помещаются во внутренний heap.
Проверочная строка успешного запуска:

```text
rom-cache=8Kx6=48K
```

Для скорости Cardputer-Adv использует mapped flash staging в ESP-IDF `storage` partition (`0x300000`, 1 MiB). Первый
запуск с новым ROM копирует и проверяет образ, повторный запуск печатает `reused mapped flash ROM ...` и
`rom-storage=flash`. На `test-rom-2.gb` (`ZELDA`, 512 KiB) smoke 2026-07-12 показал 60 headless frames за `809 ms`
(`fps=74.16`, `realtime-x=1.24`, `cache-fill=0`); `display=crop` добавляет около `211 ms` blit на 60 frames.
Если старый `settings.ini` ещё не содержит секцию `[audio]`, parser использует target default: на Cardputer-Adv это
`enabled = true`, и launcher сохранит новый ключ при следующем запуске/изменении настроек.

## Palettes

`gbemu` поддерживает named RGB565 palettes на этапе преобразования 2-bit framebuffer в display output:

```text
gbemu /mnt/sd/gb/test-rom-2.gb --preset playable --palette dmg_green
gbemu /mnt/sd/gb/test-rom-2.gb --frames 120 --palette high_contrast
```

Доступные имена:

```text
default
dmg_green
pocket_gray
gbc_blue
gbc_brown
gbc_red
gbc_pastel
high_contrast
```

`gbemu-config` создаёт settings file с секцией:

```ini
[palette]
name = dmg_green
```

Если `[palette] name = ...` задан в settings file, он используется как default palette для runtime. CLI flag
`--palette` имеет приоритет для текущего запуска.

## Проверенное поведение

Zelda-class ROM:

- запускается;
- проходит прежнее проблемное место после первого нажатия;
- не зависает в старом `STAT` polling loop;
- в основном игровом режиме работает достаточно быстро для playable prototype.

Pokémon-class ROM:

- запускается;
- проходит длинные bounded tests;
- хорошо использует `fast-halt`;
- работает быстрее realtime в idle-сценах.

## Диагностика

Для ROM/FAT стабильности используйте:

```text
gb-rom-check /mnt/sd/gb/test-rom-1.gb
gb-cart /mnt/sd/gb/test-rom-1.gb
gb-cart /mnt/sd/gb/test-rom-2.gb
gbemu /mnt/sd/gb/test-rom-1.gb --frames 2
sd-debug
mem
```

Для CPU/PPU/IO диагностики доступны отдельные команды:

```text
gb-info
gb-bus
gb-cpu-step
gb-io
gb-irq
gb-ppu-test
gb-test-run
gb-test-suite
gb-vram-view
```

## Известные ограничения

- SRAM `.sav`, save states и palette preview остаются backlog;
- full-speed `render-every=1` слишком дорог для текущей реализации;
- `gbemu` требует PSRAM/external memory policy на Pico/PicoCalc и не является частью standard PicoCalc firmware;
- Cardputer-Adv запускается без PSRAM, но оставляет около 50 KiB heap после выделения emulator workspace, поэтому
  новые фоновые сервисы надо проверять рядом с `gbemu`.
