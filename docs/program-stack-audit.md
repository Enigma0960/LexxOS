# Program stack audit for 0.0.7

Имена тестовых файлов и идентификаторы портов в примерах условные; результаты измерений сохранены.

Дата замера: 2026-06-25.
Устройство: ClockworkPi PicoCalc with Raspberry Pi Pico W.
Serial: `/dev/tty.usbserial-XXXX`, 115200 baud.
Прошивка: LexxOS 0.0.7 standard, PicoCalc Pico W.

Методика: команды запускались через supervisor (`run <command>`) или напрямую, если программа уже имеет
`ForegroundTask` runtime profile. Значение `used` рассчитано как `stack - free` из `jobs`. Для production-целей нужен
запас не меньше 1024 байт; значения ниже этого запаса помечены как кандидаты на оптимизацию.

## Measured commands

| Command | Stack | Free | Used | Notes |
|---|---:|---:|---:|---|
| `fs-smoke /mnt/sd` | 5120 | 1888 | 3232 | Optimized. Arena workspace confirmed: `int 0/781`. |
| `wifi-scan --raw --bssid --limit 32 --timeout 15000` | 5120 | 1980 | 3140 | Stabilized. Worst-case scan now has guard margin. |
| `cp /mnt/sd/cpsrc /tmp/cpdst` | 4096 | 1040 | 3056 | Optimized. FAT read + tmpfs write, arena `int 0/385`; barely above 1 KiB guard. |
| `cp /tmp/cpsrc /mnt/sd/cpdst` | 4096 | 1152 | 2944 | Optimized. tmpfs read + FAT write, arena `int 0/385`. |
| `sd-bench 1` | 5120 | 1936 | 3184 | OK. Workspace уже в arena: `int 0/640`. |
| `storage` | 4096 | 2184 | 1912 | Optimized. Worst measured `storage diag`, arena `int 0/513`. |
| `cat /mnt/sd/catsrc` | 4096 | 1648 | 2448 | Optimized. FAT path with arena `int 0/225`. |
| `hexdump /mnt/sd/audit.txt` | 4096 | 1824 | 2272 | Optimized. Path/row buffers in arena `int 0/145`. |
| `psram-test --length 16` | 4096 | 2552 | 1544 | Optimized. Three 256-byte test buffers in internal arena `int 0/771`. |
| `write /mnt/sd/audit.txt hello` | 4096 | 1728 | 2368 | Optimized. Shared text-writer arena `int 0/225`. |
| `append /mnt/sd/audit.txt world` | 4096 | 1608 | 2488 | Optimized. Shared text-writer arena `int 0/225`. |
| `ls /mnt/sd` | 4096 | 2000 | 2096 | Optimized. Path buffer in arena `int 0/128`. |
| `mv /mnt/sd/mvsrc /mnt/sd/mvdst` | 4096 | 1944 | 2152 | Optimized. Source/target path buffers in arena `int 0/256`. |
| `mount /dev/sd0 /mnt/sd` | 4096 | 1640 | 2456 | Optimized. Real SD remount path, arena `int 0/256`. |
| `mount` | 4096 | 2336 | 1760 | Optimized. Mount table listing path, arena `int 0/256`. |
| `umount /mnt/sd` | 4096 | 2388 | 1708 | Optimized. Path buffer in arena `int 0/128`. |
| `touch /mnt/sd/touchx` | 4096 | 1744 | 2352 | Optimized. Path buffer in arena `int 0/128`. |
| `mkdir /mnt/sd/dirx` | 4096 | 2008 | 2088 | Optimized. Path buffer in arena `int 0/128`. |
| `rmdir /mnt/sd/dirx` | 4096 | 2136 | 1960 | Optimized. Path buffer in arena `int 0/128`. |
| `rm /mnt/sd/touchx` | 4096 | 2208 | 1888 | Optimized. Path buffer in arena `int 0/128`. |
| `stat /mnt/sd/touchx` | 4096 | 2260 | 1836 | Optimized. Path buffer in arena `int 0/128`. |
| `sd-test` | 4096 | 1392 | 2704 | Optimized. Path/readback buffers in arena `int 0/225`; close but above 1 KiB guard. |
| `sd-stress 1 1` | 5120 | 2120 | 3000 | Optimized. Path and two 512-byte buffers in arena `int 0/1155`. |
| `board` | 4096 | 2048 | 2048 | Low/medium. |
| `backlight` | 4096 | 2184 | 1912 | Low. |
| `picocalc-sb` | 4096 | 2272 | 1824 | Low. |
| `console-info` | 4096 | 2352 | 1744 | Low. |
| `fs-info` | 4096 | 2424 | 1672 | Low. |
| `display-power` | 4096 | 2488 | 1608 | Low. |
| `devices` | 4096 | 2512 | 1584 | Low. |
| `edit --help` | 4096 | 2512 | 1584 | Help path only; real editor session still needs interactive check. |
| `version` | 4096 | 2576 | 1520 | Low. |
| `tmp-info` | 4096 | 2576 | 1520 | Low. |
| `tasks` | 4096 | 2600 | 1496 | Low. |
| `block-devs` | 4096 | 2608 | 1488 | Low. |
| `psram-info` | 4096 | 2656 | 1440 | Low. |
| `display-info` | 4096 | 2664 | 1432 | Low. |
| `mem` | 4096 | 2736 | 1360 | Low. |
| `pwd` | 4096 | 2800 | 1296 | Low. |

## Optimization queue

| Priority | Command | Current risk | Next change | Stack target |
|---|---|---|---|---:|
| P0 | `fs-smoke` | Optimized run uses about 3232 bytes with 5120 stack. | Keep `5120`. `4096` would leave only about 864 bytes, below the current guard. | 5120 verified |
| P1 | `wifi-scan` | Worst-case raw/BSSID scan used about 3140 bytes. | Keep `5120`. Do not move `WifiScanState` to per-process arena until the CYW43 callback timeout/lifetime path has an owner that outlives the process. | 5120 verified |
| P1 | `cp` | Optimized successful runs leave 1040-1152 bytes free on 4096 stack. | Keep `4096`; do not try 3584 until copy path is structurally simplified or guard target changes. | 4096 verified |
| P1 | `sd-bench` | Uses about 3184 bytes with 5120 stack. | Current arena workspace is working. Keep 5120 until cancellation/background runs are remeasured; then try 4096. | 5120 now, 4096 candidate |
| P1 | `sd-stress` | Optimized short run uses about 3000 bytes with 5120 stack. | Keep `5120` until default `sd-stress` and cancellation paths are measured; then consider `4096` if guard stays above 1024. | 5120 verified |
| P2 | `storage` | Optimized `storage diag` leaves 2184 bytes free on 4096 stack. | Keep `4096`; lower only after broader SD diagnostics are measured. | 4096 verified |
| P2 | `sd-test` | Optimized run leaves 1392 bytes free on 4096 stack. | Keep `4096`; this is above the 1 KiB guard, but too close for a lower stack target. | 4096 verified |
| P2 | `cat` | Optimized FAT read leaves 1648 bytes free on 4096 stack. | Keep `4096`; stdin path still needs a separate redirection check. | 4096 verified |
| P2 | `write`, `append` | Optimized writes leave 1608-1728 bytes free on 4096 stack. | Keep `4096`; stdin-copy path still needs a redirected-input check. | 4096 verified |
| P2 | `hexdump`, `ls` | Optimized commands leave 1824-2000 bytes free on 4096 stack. | Keep `4096`; stdin path for `hexdump` still needs a redirected-input check. | 4096 verified |
| P2 | `psram-test` | Optimized run leaves 2552 bytes free on 4096 stack. | Keep `4096`; command remains `InternalOnly` because it tests PSRAM rather than consuming PSRAM workspace. | 4096 verified |
| P2 | `stat`, `touch`, `rm`, `mkdir`, `rmdir` | Optimized single-path VFS commands leave 1744-2260 bytes free on 4096 stack. | Keep `4096`; these are good lower-stack candidates only after the inline supervisor minimum is split. | 4096 verified |
| P2 | `mv`, `mount`, `umount` | Optimized runs leave 1640-2388 bytes free on 4096 stack; real `mount /dev/sd0 /mnt/sd` is the deepest path. | Keep `4096`; `mount` should stay at this target because SD probe/remount uses the deepest VFS/storage path. | 4096 verified |
| P3 | simple diagnostics (`version`, `mem`, `pwd`, `tasks`, `devices`, `block-devs`, `display-info`) | More than 1.4 KiB free on 4096 stack. | Safe candidates for a lower inline supervisor stack once exceptions for VFS/storage commands exist. | 3072 |

## Static candidates not covered by standard firmware

- `gbemu`: not registered in the standard no-PSRAM build. It remains the main external-memory target: `ExternalRequired`,
  1024 KiB external quota, current stack profile 12288. The launcher can call into the emulator before returning, so
  this profile keeps headroom for nested TUI/emulation frames.
- ESP-IDF ESP-Hosted `wifi-scan`: measured separately on `jc_esp32p4_m3_dev` after C6 firmware update to ESP-Hosted
  2.11.7. The universal ESP-IDF command uses stack `8192`; the `5120` audit row above applies to the Pico/CYW43
  diagnostic command.
- Game Boy diagnostics in full builds: `gb-rom-check` has a 512-byte block buffer and several probe arrays; move its
  block buffer to arena before treating it as a normal supervised command.
- `mplayer`: disabled by default, but MIDI player state and track arrays should be audited before enabling in standard
  PicoCalc builds.

## Runtime policy targets

- Keep the global supervised minimum at 4096 until heavy inline commands have explicit profiles.
- Add per-command runtime profiles for heavy diagnostics instead of lowering every command together.
- After `fs-smoke`, `cp`, and `sd-bench` arena moves are verified on hardware, introduce a smaller `Inline` supervisor
  stack target of 3072 for simple commands.
- Keep task stack and TCB in internal SRAM; PSRAM should hold program workspaces, file buffers, ROM/cache pages, and
  editor documents, not FreeRTOS stacks.
