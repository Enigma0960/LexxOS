# `gbemu`

run Game Boy ROMs or open the ROM launcher.

## Cardputer-Adv

The local `user-espidf-m5stack_cardputer` preset enables `gbemu` on Cardputer-Adv without PSRAM. The ESP-IDF component
propagates `LEXXOS_GAMEBOY_ROM_CACHE_BANKS=3`, `LEXXOS_GAMEBOY_ROM_CACHE_PAGE_BYTES=8192` and dynamic ROM cache
allocation; a correct runtime diagnostic prints `rom-cache=8Kx6=48K`.

For playable runs Cardputer-Adv defaults to mapped flash ROM staging, so SD/FAT reads stay out of the emulator hot
path. The staging area is the ESP-IDF `storage` data partition at `0x300000` with 1 MiB capacity; first use copies and
verifies the ROM, and later runs print `reused mapped flash ROM ... rom-storage=flash`. This partition is treated as raw
Game Boy staging data by `gbemu`.

Quick smoke commands:

```text
lexx:/> gb-info /mnt/sd/gb/test-rom-2.gb
lexx:/> gbemu --profile --frames 60 --display none /mnt/sd/gb/test-rom-2.gb
lexx:/> gbemu --audio --profile --frames 600 --display none /mnt/sd/gb/test-rom-2.gb
lexx:/> gbemu --frames 60 --display crop /mnt/sd/gb/test-rom-2.gb
```

The Cardputer keyboard is exposed as `cardputer-gamepad` for emulator input:

```text
e up, z down, a left, d right
[ A, ] B, ; X, ` Y
Enter Start, Backspace Select
```

## TUI launcher

`gbemu` без аргументов сканирует `[general] rom_dir` из `/mnt/sd/apps/gbemu/settings.ini` (по умолчанию
`/mnt/sd/gb`), показывает `.gb`/`.gbc` файлы и сохраняет последний выбранный ROM в `last_rom`.

В Settings есть переключатель `Audio`. Если он включён, launcher передаёт выбранный ROM в emulator path с `--audio`;
это отличается от прямого ESP-IDF shell-запуска `gbemu <rom>`, где звук включается только явным `--audio`.

В Settings также есть блок `Advanced / experimental`: `speed_mode` (`playable`, `playable-fast`, `max-speed`,
`psram-fast`, `manual`), `frames` (`0` означает запуск без лимита кадров), `steps_per_frame`, `cycles_per_frame`,
warmup-режимы, `rom_storage` и `no_ppu`.
CLI-аргументы имеют приоритет над этими значениями.

Клавиши:

```text
Up/Down     select
A / Enter   run ROM or edit setting
Left/Right  change setting value
Start       settings
B / Escape  back or exit ROM list
Select / Backspace
            exit ROM list
Start+Select held in emulation exits the ROM (Tab+Backspace on keyboard controls)
```

## Справка shell

```text
gbemu - run Game Boy ROMs or open the ROM launcher

Without arguments opens a TUI ROM launcher. With a ROM path, loads a Game Boy ROM, runs a bounded CPU/bus/timer/joypad loop, renders the current background and prints compact runtime diagnostics.

Usage:
  gbemu [--frames count] [--steps-per-frame count] [--cycles-per-frame count] [--real-frame] [--until-vram] [--until-visible] [--until-tile-data] [--until-bg-ready] [--min-tile-data count] [--max-cycles count] [--force-visible] [--bg-map auto|9800|9c00] [--bg-tiles auto|8000|8800] [--display crop|fit|none] [--palette name] [--render-every count] [--input-every count] [--preset none|playable] [--playable-fast] [--max-speed] [--psram-fast] [--rom-flash] [--rom-flash-reuse] [--rom-psram] [--rom-psram-reuse] [--audio] [--no-audio] [--no-ppu] [--fast-timing] [--sync-realtime] [--profile] [--profile-every count] [--no-fast-halt] [--break-pc address] [--break-pc-from address] [--watch-blank] [--watch-blank-after count] [--watch-blank-streak count] [--break-bgp-zero-after count] [--fault-trace] [--trace] [rom]

Arguments:
  --audio - enable Game Boy APU PCM playback on targets where it is disabled by default
  --no-audio - disable Game Boy APU PCM playback even when a PCM audio device is available
  -f, --frames <count> - number of emulated frames to run before returning to shell; 0 runs until cancel/exit combo; default comes from settings.ini
  -s, --steps-per-frame <count> - maximum CPU instructions to execute before each rendered frame; default comes from settings.ini
  -c, --cycles-per-frame <count> - target emulated CPU cycles per rendered frame; 0 keeps steps-per-frame mode; default comes from settings.ini
  -t, --trace - print one compact runtime line after each rendered frame
  --fault-trace - debug: keep a rolling opcode trace and dump it when gbemu detects a suspicious state
  --watch-blank - debug: stop and dump CPU/PPU state when a rendered frame becomes fully white after visible frames
  --watch-blank-after <count> default=0 - debug: ignore blank-frame detection before the given emulated frame
  --watch-blank-streak <count> default=1 - debug: require this many rendered blank frames before stopping
  --break-bgp-zero-after <count> default=0 - debug: stop when BGP becomes 0 after the given emulated frame
  -p, --profile - debug: print timing breakdown for gbemu stages without per-frame opcode trace
  --profile-every <count> - debug: print one profile line after every N emulated frames; default comes from settings.ini
  --real-frame - use the original DMG frame budget (70224 cycles) when cycles-per-frame is 0
  --sync-realtime - limit playback to the original Game Boy realtime speed when cycle-based frame timing is enabled
  --until-vram - fast-forward before the first render until the ROM writes to VRAM
  --until-visible - fast-forward before the first render until any VRAM byte is non-zero
  --until-tile-data - fast-forward before the first render until VRAM tile data 0x8000-0x97FF is non-zero
  --until-bg-ready - fast-forward before the first render until BG tile map and tile data are populated
  --min-tile-data <count> - minimum non-zero tile-data bytes used by --until-bg-ready; default comes from settings.ini
  --max-cycles <count> - maximum fast-forward cycles used by warmup options; default comes from settings.ini
  --force-visible - debug: force LCD enable, BG enable and DMG palette before rendering
  --bg-map <auto|9800|9c00> default=auto - debug: override background tile-map base before rendering
  --bg-tiles <auto|8000|8800> default=auto - debug: override background tile-data addressing before rendering
  --display <crop|fit|1x|none> - display mapping: crop/fit scale to the LCD, 1x centers the native 160x144 frame, none runs without blitting; default comes from settings.ini
  --palette <name> - RGB565 palette: default, dmg_green, pocket_gray, gbc_blue, gbc_brown, gbc_red, gbc_pastel or high_contrast; default comes from settings.ini
  --render-every <count> - blit only every N emulated frames and always blit the final frame; default comes from settings.ini
  --input-every <count> - poll gamepad only every N emulated frames; skipped frames reuse the previous state; default comes from settings.ini
  --preset <none|playable> - apply a known playback preset for the current run; settings.ini speed_mode provides the default launcher/direct-run mode
  --playable-fast - shortcut for visual playback: real DMG frame budget and render every 2nd frame unless overridden
  --max-speed - PicoCalc performance shortcut: real-frame, render every 2nd frame, and reuse/program XIP flash ROM staging unless ROM storage is explicitly selected
  --psram-fast - PicoCalc safe performance shortcut: real-frame, render every 2nd frame, and reuse/copy ROM through PSRAM unless ROM storage is explicitly selected
  --rom-flash - copy ROM to the reserved RP2xxx XIP flash staging area before emulation; removes SD/FAT reads from the hot path
  --rom-flash-reuse - reuse the reserved RP2xxx XIP flash staging area when it already contains the same ROM; otherwise program it
  --rom-psram - copy ROM to PicoCalc PSRAM before emulation; cache misses are served from PSRAM instead of SD/FAT
  --rom-psram-reuse - reuse PicoCalc PSRAM when it already contains the same ROM; otherwise copy it from SD/FAT
  --no-ppu - debug: skip framebuffer rendering entirely; useful for pure CPU/bus profiling
  --fast-timing - experimental: use VBlank-only lightweight PPU timing during gbemu playback
  --break-pc <address> - debug: stop the current frame as soon as CPU reaches the given PC, decimal or 0x-prefixed
  --break-pc-from <address> - debug: stop when CPU PC becomes greater than or equal to the given address; useful for catching jumps into RAM/HRAM
  --no-fast-halt - debug: disable HALT fast-forwarding used by gbemu
  rom - Game Boy ROM image path; without a ROM gbemu opens the launcher

Runtime:
  launch: foreground-task
  stack: 12288 bytes
  priority: 2
  memory: external-preferred
  interface: graphical
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
