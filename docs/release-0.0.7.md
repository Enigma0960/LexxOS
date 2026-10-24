# LexxOS 0.0.7 release readiness

Имена тестовых файлов и идентификаторы портов в примерах условные; результаты измерений сохранены.

Дата ревизии: 2026-07-08. Исторический документ: версии и результаты ниже
относятся к 0.0.7. Текущий выпуск описан в [0.1.0](release-0.1.0.md).

## Статус

`CMakeLists.txt` и ESP-IDF project metadata уже выставляют версию `0.0.7`. Основной release target для этой версии —
`picocalc_pico_w`: ClockworkPi PicoCalc с Raspberry Pi Pico W считается поддержанным автономным устройством LexxOS.

Ключевые завершённые изменения этого цикла:

- boot/LCD console path стабилизирован: загрузочные сообщения, приветствие, prompt, UART/USB/LCD mirror идут через общий
  вывод;
- асинхронные console-сообщения от Wi-Fi, system tasks и FreeRTOS task startup теперь вставляются выше активной строки
  ввода shell и перерисовывают сохранённый prompt/input после перевода строки;
- PicoCalc LCD console разложена на layout, terminal state, scrollback navigation, status bar и renderer components;
- PicoCalc keyboard driver разложен на controller, transport, initializer, FIFO poller, input state, event bridge,
  diagnostics и runtime service;
- PicoCalc southbridge разделён на snapshot cache, backlight mapper, safe requests, command tracker и resource policy;
- `DeviceManager` показывает device/service resource status, priority и owner;
- PicoCalc audio path имеет `AudioDevice`, `beep` и первый PWM mixer для `audio`/опционального `mplayer`;
- Wi-Fi path прошёл дальше scan-only этапа: есть `wifi-config`, boot auto-connect, `wifi-connect`, `wifi-status`,
  `ping` и `rtc-sync`;
- FatFs является единственным FAT backend; legacy FAT implementation удалён;
- VFS state перенесён в экземпляр `lexxos::vfs::Vfs`; `OperatingSystem::vfs()`/`AppContext::vfs()` стали явным
  runtime path, а free functions сохранены как compatibility adapter над `defaultVfs()`;
- введён `lexxos::lifecycle::EventBus`: SD hot-plug, input и power/shutdown requests публикуются типизированными
  событиями, а тяжёлая работа остаётся в task context;
- фиксированные лимиты portable runtime собраны в `lexxos::runtime::RuntimeProfile`: registry capacities,
  VFS/mount/tmpfs limits, event queues, системные stack sizes и дефолтные optional workspaces;
- `sd-speed-scan` разбит на helper-модули для argument parsing, raw/FAT probe, settings guard и report formatting;
- табличный вывод shell-команд переведён на `apps/common/pretty_table.*`;
- runtime программ использует `ProgramRuntimeProfile`, `ProgramSupervisor`, `jobs`, cooperative `stop` и per-process
  `ProgramMemoryArena`;
- PicoCalc Pico W получил отдельный RP2040 tight-RAM profile: 88 KiB FreeRTOS heap, Game Boy/MIDI/GPIO analyzer
  выключены в релизной сборке, а `display-test --bench` перенёс RGB565 chunk buffers из static `.bss` в lazy heap
  allocation без descriptor-level quota для обычного `display-test`;
- экспериментальный `.lexx` runtime доведён до минимального 0.0.7-сценария: прямой запуск внешних RV32I-программ с SD/VFS,
  `lexx-run`, `argc/argv`, keyboard/console ABI и read-only storage ABI через capability `storage-read`;
- PicoCalc и storage команды проверены на стековые профили и перенесли крупные рабочие буферы в arena.
- Game Boy full-track получил `--preset playable`/`--preset none`, runtime defaults из
  `/mnt/sd/apps/gbemu/settings.ini`, TUI launcher с advanced speed settings, named RGB565 palettes через `--palette`,
  тихий default output без per-frame diagnostics и отдельный parser test для settings.

## Проверено локально

Последний полный локальный прогон после переноса табличного вывода:

```text
git diff --check
cmake --build build/host-full
ctest --test-dir build/host-full --output-on-failure
cmake --build build/sim-full
LEXXOS_SIM_LIB=build/sim-full/src/liblexxos_sim.dylib python3 -m pytest emulator/tests -k 'not gbemu'
cmake --build build/host-midi
ctest --test-dir build/host-midi --output-on-failure
cmake --build --preset custom_configurator-build
cmake --build --preset debug-picocalc-pico-w
cmake --build --preset lexxos-picocalc-pico2w
cmake --build --preset debug-stm32f407
```

Ожидаемый известный warning остаётся только у STM32F407 linker: `LOAD segment with RWX permissions`.

После пересмотра C++ layout policy дополнительно пройден release-gate с включёнными static checks:

```text
cmake -S . -B build/host-checks -DLEXXOS_PLATFORM=host -DLEXXOS_BOARD=host_native -DLEXXOS_ENABLE_STATIC_CHECKS=ON
cmake --build build/host-checks --target lexxos_contract_tests lexxos_system_contract_tests lexxos_services_contract_tests
ctest --test-dir build/host-checks --output-on-failure
```

Результат: `lexxos_cpp_layout_check`, `lexxos_markdown_check` и host contract tests прошли. Строгий аудит
implementation-local `struct`/`class` оставлен отдельным opt-in режимом:
`python3 tools/check_cpp_layout.py . --forbid-cpp-types`.

После переноса VFS state в `Vfs` и ввода lifecycle event bus дополнительно пройдены:

```text
cmake --build build/host-full
ctest --test-dir build/host-full --output-on-failure
cmake --build build/sim-full
cmake --build --preset debug-picocalc-pico-w
```

Для `.lexx` runtime-правок от 2026-07-08 локально пройдены:

```text
make -C examples/lexx/hello clean all
make -C examples/lexx/cat clean all
cmake --build build/host-full --target src/CMakeFiles/lexxos.dir/lexxos/external_program/external_program_runner.cpp.o
cmake --build build/host-full --target src/CMakeFiles/lexxos.dir/lexxos/apps/lexx/lexx_run_program.cpp.o
cmake --build build/host-full
ctest --test-dir build/host-full --output-on-failure
```

Host-full build blocker `picoCalcConsoleMirrorTaskPriority` закрыт: console mirror task priority теперь объявлен для
любого `FEATURE_DISPLAY`, а `lexxos_services_contract_tests` получает `program_input` sources, нужные для `shell.cpp`.

Релизная проверка code-style от 2026-07-08:

```text
/opt/homebrew/Cellar/llvm/22.1.7_1/bin/clang-format --version
git ls-files -z '*.cpp' '*.h' '*.hpp' '*.c' '*.cc' '*.cxx' ':!:third_party/**' ':!:build/**' ':!:cmake-build-*' | xargs -0 /opt/homebrew/Cellar/llvm/22.1.7_1/bin/clang-format --dry-run --Werror
python3 tools/check_cpp_layout.py .
python3 tools/check_markdown.py .
```

Результат: собственные C/C++ sources приведены к `.clang-format`, обычный C++ layout gate и Markdown gate проходят.
Строгий аудит implementation-local `class`/`struct` пересмотрен как диагностический режим старого Stage 0 правила:
локальные helper-типы в `.cpp` разрешены текущей политикой и не считаются живым 0.0.7 backlog.

После code-style cleanup и явного подавления `nodiscard` warning в shell cancellation path повторно пройден host-check
набор:

```text
cmake -S . -B build/host-checks -DLEXXOS_PLATFORM=host -DLEXXOS_BOARD=host_native -DLEXXOS_ENABLE_STATIC_CHECKS=ON
cmake --build build/host-checks --target \
  lexxos \
  lexxos_contract_tests \
  lexxos_system_contract_tests \
  lexxos_gameboy_settings_tests \
  lexxos_vfs_file_string_tests \
  lexxos_services_contract_tests
ctest --test-dir build/host-checks --output-on-failure
```

Результат: `100% tests passed, 0 tests failed out of 13`, включая `lexxos_board_preset_check`,
`lexxos_gameboy_settings_tests` и `lexxos_vfs_file_string_tests`.

Command docs regeneration после `--preset playable`/`--preset none`, `--palette`, settings-backed help text и
preserve-existing поведения генератора:

```text
cmake --build build/host-full --target lexxos
cmake --build build/sim-full --target lexxos_sim
python3 tools/generate-command-docs.py --host build/host-full/src/lexxos --sim-lib build/sim-full/src/liblexxos_sim.dylib --output docs/commands
```

Результат: сгенерирован 91 runtime command document, сохранено 12 профильных страниц, общий индекс не теряет
PicoCalc/Wi-Fi/PSRAM/MIDI-only команды. Disabled-команды вроде `gbemu` обновляются через targeted `help <name>`.

Дополнительный Game Boy TODO cleanup от 2026-07-08:

```text
git diff --check
git ls-files -z '*.cpp' '*.h' '*.hpp' '*.c' '*.cc' '*.cxx' \
  ':!:third_party/**' ':!:build/**' ':!:cmake-build-*' \
  | xargs -0 /opt/homebrew/Cellar/llvm/22.1.7_1/bin/clang-format --dry-run --Werror
python3 tools/check_cpp_layout.py .
python3 tools/check_markdown.py .
python3 tools/check_board_presets.py .
cmake -S . -B build/host-checks \
  -DLEXXOS_PLATFORM=host \
  -DLEXXOS_BOARD=host_native \
  -DLEXXOS_ENABLE_STATIC_CHECKS=ON
ctest --test-dir build/host-checks --output-on-failure
cmake --build build/host-full --target lexxos lexxos_gameboy_settings_tests
ctest --test-dir build/host-full --output-on-failure
cmake --build build/sim-full --target lexxos_sim
LEXXOS_SIM_LIB=build/sim-full/src/liblexxos_sim.dylib python3 -m pytest emulator/tests/test_virtual_board.py emulator/tests/test_sd.py -q
```

Результат: host-checks `13/13`, host-full CTest `10/10`, simulator smoke `7 passed`, command docs снова
перегенерированы: 91 command document, 12 preserved profile pages.

После обнаруженного RP2040 SRAM overflow в сборке первого Pico W дополнительно пройдены:

```text
cmake --build build/configurator/picocalc-pico-w --target lexxos
arm-none-eabi-size build/configurator/picocalc-pico-w/src/lexxos.elf
cmake --preset debug-picocalc-pico2w
cmake --build --preset lexxos-picocalc-pico2w
cmake --build build/host-full --target lexxos
```

Результат: `picocalc_pico_w` линкуется с `LEXXOS_PICO_FREERTOS_HEAP_KB=88`, `LEXXOS_FEATURE_GAMEBOY=OFF`,
`LEXXOS_FEATURE_MIDI=OFF`, `LEXXOS_FEATURE_GPIO_ANALYZER=OFF`. Map-файл показывает `.data=0x106a8`, `.bss=0x2d3e0`,
FreeRTOS `ucHeap=0x16000`, `.heap` заканчивается до `.stack1_dummy 0x20040000`; этот UF2 ушёл в hardware retest на
реальном PicoCalc Pico W.

Hardware retest 2026-07-08 на первом Pico W после tight-RAM split подтвердил boot, `version`, `board`, `mem`, `top`,
SD/FAT и display info, но обычный `display-test` упал в `panic: FreeRTOS malloc failed`: прежний descriptor резервировал
около 80 KiB benchmark workspace до разбора режима. После этого source fix убрал quota из descriptor, перевёл benchmark
buffers на lazy heap allocation и снизил дефолт `--rows` до 16. Новый UF2 после этого фикса нужно прошить и повторить
`display-test`/`display-test --bench`.

Постфиксная локальная сборка `picocalc_pico_w` прошла: `arm-none-eabi-size` показывает `text=835000`, `data=0`,
`bss=187552`, а новый UF2 собран как `build/configurator/picocalc-pico-w/src/lexxos.uf2`. UART после старого panic
вернул `NO_RESPONSE`, поэтому следующий hardware retest начинается с power cycle/reflash.

Повторный hardware retest после lazy workspace fix подтвердил обычный `display-test`, full-width double-buffer benchmark,
`transport all` на GB1, graceful heap refusal для `--rows 64`, SD/FAT, southbridge, Wi-Fi, `rtc-sync --tz +07:00` и soft
reboot. Новый найденный blocker: PSRAM data path на первом Pico W при 16 MHz из старого CMake cache даёт readback
mismatch (`expected=A5 actual=D2`) и restore mismatch. После этого CMake cap снижает
`LEXXOS_PICOCALC_PSRAM_SPI_BAUDRATE` до 8 MHz для `picocalc_pico_w`; UF2 пересобран и требует отдельного reflash/PSRAM
retest.

После отчёта о перекрытии активной строки ввода сообщениями `wifi-autoconnect`/`FreeRTOS task start` добавлен
line-aware guard в `console_output`: внешний вывод очищает текущую prompt line, печатает chunked-сообщение и после `\n`
перерисовывает сохранённый `lexx:<cwd>> <input>`. Локально пройдены:

```text
cmake --build build/host-full --target lexxos_services_contract_tests
./build/host-full/tests/cpp/lexxos_services_contract_tests
ctest --test-dir build/host-full --output-on-failure
cmake --build build/host-full --target lexxos
cmake --build build/configurator/picocalc-pico-w --target lexxos
cmake --build --preset lexxos-picocalc-pico2w
python3 tools/check_markdown.py .
python3 tools/check_cpp_layout.py .
python3 tools/check_board_presets.py .
git diff --check
```

Результат: host CTest `10/10`, Pico W/Pico 2 W firmware targets собраны. Свежий Pico W ELF после UX-фикса:
`text=836160`, `data=0`, `bss=187576`; Pico 2 W: `text=846500`, `data=0`, `bss=323932`. UART UX нужно подтвердить на
реальном PicoCalc после прошивки нового UF2.

Следующий UART input retest на текущей прошивке воспроизвёл ещё два shell UX дефекта: быстрый paste в UART доходил
только до первых 32 байт (`echo 0123456789ABCDEFGHIJKLMNOPQ`), а path completion для длинного MIDI LFN останавливался
на 32-байтном имени вроде `ExampleMidiFile_Theme_One_Long1.`. Source fix:

- `console_input::readAvailable()` и console task batch-drain сначала быстро вычитывают доступные UART/input байты в RAM,
  а уже потом отдают их shell line editor на echo и обработку;
- `VfsRuntimeLimits::maxNameLength` поднят до 64, что соответствует текущему FatFs `FF_LFN_BUF=64` и позволяет shell
  completion держать длинные ASCII LFN в пределах `maxPathLength`;
- добавлены host regression tests на paste-sized input batch и single-match completion имени
  `ExampleMidiFile_Finale_With_A_Long_Title.mid`.

Локально повторно пройдены `lexxos_contract_tests`, `lexxos_services_contract_tests`, host CTest `10/10`, host/Pico W/Pico
2 W builds и markdown/C++ layout/board preset/diff checks. Свежий Pico W ELF после input/LFN fix: `text=836400`,
`data=0`, `bss=188056`; Pico 2 W: `text=846636`, `data=0`, `bss=324412`. Hardware UART paste/completion retest нужен
после прошивки нового UF2.

## Проверено на PicoCalc

Аппаратный serial smoke 2026-07-02 на ClockworkPi PicoCalc with Raspberry Pi Pico W подтвердил свежую прошивку
`version: 0.0.7`, SD/FAT automount, soft reboot без потери serial port, `PrettyTable` без stack overflow, audio beep на
реальном динамике, PSRAM probe/test, Wi-Fi boot auto-connect, `wifi-status`, DNS/ICMP через
`ping --count 1 pool.ntp.org` и supervisor `jobs`.

Аппаратный UART smoke 2026-07-08 на ClockworkPi PicoCalc with Raspberry Pi Pico 2 W подтвердил профиль
`picocalc_pico2w`: `version`, `board`, SD/FAT, LCD console, southbridge keyboard/backlight/display-power, RTC/NTP
`rtc-sync --tz +07:00`, PSRAM JEDEC/data test, beep/audio, Wi-Fi scan/status/ICMP, `.lexx` external programs, soft
reboot и `poweroff`. Подробности зафиксированы в
[`docs/tests/picocalc-pico2w-0.0.7-regression.md`](tests/picocalc-pico2w-0.0.7-regression.md).

Найденные хвосты и решения перед релизной меткой:

- `rtc-sync` на прошитом 2026-07-02 Pico W образе доходил до NTP request, но PicoCalc JCS RTC возвращал
  `rtc-sync: failed to write RTC date/time`; source fix добавил readback fallback для тикающего RTC time echo, а
  Pico 2 W smoke 2026-07-08 подтвердил `rtc sync: ok`;
- первый Pico W/RP2040 больше не собирается с heavy feature set из GUI/cache: release profile принудительно держит
  tight-RAM набор, hardware retest выявил и закрыл source-level panic в `display-test`, а PSRAM baud теперь
  release-capped на 8 MHz после найденного 16 MHz readback mismatch;
- периодический `[picocalc-keyboard] communication failure: stage=update-keyboard-fifo-read` оформлен как known
  non-blocking warning для JCS: на PicoCalc Pico 2 W 2026-07-11 он наблюдался и в 2-core SMP, и в 1-core control, при
  `ready=yes`, `consecutive=1`, `snapshot-ok=yes`; устройство, SD/FAT, shell и `gbemu` продолжали работать.

## Перед релизной меткой

Минимальный список, который ещё нужно закрыть перед tag/release notes:

- прошить новый `picocalc_pico_w` UF2 после PSRAM 8 MHz cap и повторить минимум `version`, `board`, `psram-info`,
  `psram-test`, `display-test`, `display-test --bench`, SD/FAT и Wi-Fi;
- на том же новом UF2 проверить UART prompt redraw: набрать часть строки до `wifi-autoconnect`/task startup output и
  убедиться, что сообщение вставляется выше, а введённый текст остаётся на prompt line;
- на том же новом UF2 проверить быстрый UART paste длинной строки и Tab completion для длинных MIDI LFN в
  `/mnt/sd/midi`;
- сформировать release notes с точной формулировкой поддержки: `PicoCalc Pico W поддержан`; `PicoCalc Pico 2 W` прошёл
  hardware smoke на реальном PicoCalc.

## Не блокирует 0.0.7

Эти пункты остаются в roadmap, но не должны задерживать релиз, если release notes называют их явно:

- PSRAM allocator/cache/arena backend;
- активная low-battery policy;
- Bluetooth runtime API;
- MIDI `mplayer` в standard build;
- STM32F407 RWX linker warning;
- ProgramModule registry и board-specific runtime profile variants;
- Game Boy graphical ROM launcher, SRAM saves, save states, palette settings UI/preview and APU;
- JCS `update-keyboard-fifo-read` warning, если он одиночный и сопровождается `ready=yes`, `consecutive=1`,
  `snapshot-ok=yes` без потери ввода.
