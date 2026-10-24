# Архитектурная дорожная карта LexxOS

Состояние перенесено из старого архитектурного плана. Этот документ хранит основной архитектурный контекст, закрытые
этапы и целевую модель. Краткий список активных задач находится в [`roadmap.md`](../roadmap.md).

Активные задачи ведутся только в корневом roadmap. Этапы ниже сохраняют архитектурный контекст;
их формулировки не являются отдельным актуальным release checklist.

## Цель

Подготовить LexxOS к добавлению новых плат, драйверов, файловых систем и программ без разрастания центральных
`#if`, singleton-зависимостей и ручных списков инициализации.

Целевая архитектура должна оставаться пригодной для MCU:

- без обязательного heap allocation;
- без RTTI и exceptions;
- без тяжёлого dependency injection framework;
- со статической компоновкой по умолчанию;
- с явными лимитами памяти и предсказуемым startup;
- с host/simulator-тестами тех же контрактов, которые используются на плате.

Динамическая загрузка модулей не является целью первых этапов. Сначала нужны устойчивые контракты и composition root.

## Текущее состояние

Сильные стороны проекта:

- platform, board profile, devices, VFS, filesystem и apps разделены по каталогам;
- драйверы работают через переносимые интерфейсы `I2cBus`, `SpiBus`, `BlockDevice`, `DisplayDevice` и `InputDevice`;
- VFS скрывает FAT и tmpfs за общим API;
- команды имеют `ProgramDescriptor`;
- simulator проверяет shell, VFS, storage, display, input и Game Boy;
- добавлен ESP-IDF port для ESP32/M5Stack Basic с UART shell, LCD, кнопками, SPI SD/FatFs automount и ESP32
  diagnostics;
- CMake использует явные списки исходников по модулям;
- быстрые host contract tests и статические layout/Markdown checks подключены через CTest.

Основные точки связанности:

1. `Runtime` одновременно является composition root, lifecycle manager и владельцем optional services.
2. VFS, platform API и legacy device access доступны через глобальные функции.
3. Runtime composition layer через `composition/board_device_providers.cpp` знает конкретные Pico, STM32, ESP-IDF и
   simulator-драйверы и содержит platform/feature условия вне core `DeviceManager`.
4. `BoardDescriptor` содержит поля конкретных периферийных профилей (`SPI TFT`, `PCF8575`, parallel TFT), поэтому общий
   board
   contract растёт с каждым новым устройством.
5. Регистрация встроенных команд теперь генерируется из `cmake/commands.json`; группы и зависимости описаны в [матрице](command-matrix.md).
6. FAT сам обращается к глобальному VFS для поиска block device и регистрации mount.
7. `RuntimeProfile` уже собирает фиксированные лимиты portable runtime; следующий шаг: profile selection по board/target
   и capability registry вместо scattered feature checks.
8. Локальные host checks покрывают быстрые C++ contracts и базовые layout-правила, но ещё не заменяют Python
   simulator regression suite.

## Открытый технический долг

Повторный аудит зафиксировал задачи, которые не блокируют завершённый структурный рефакторинг, но остаются
самостоятельными техническими хвостами:

- предупреждение STM32 linker о LOAD-сегменте с правами RWX;
- experimental CMD25 и guarded RP2040 SDIO skeleton;
- отсутствие write-back для режима Python emulator `--sd-dir`;
- отсутствие repair-mode у FAT diagnostics.

Пункты про experimental CMD25/SDIO, emulator write-back и FAT repair-mode являются отдельными функциональными задачами
и не должны смешиваться с миграцией composition root.

## Целевая модель

```text
target composition root
        |
        +-- PlatformServices
        |     console, clock, gpio, reboot, flash regions
        |
        +-- RuntimeServices
        |     scheduler, event bus, lifecycle
        |
        +-- DeviceRegistry
        |     DriverProvider[] + board component declarations
        |
        +-- Vfs
        |     node providers + filesystem factories + mount table
        |
        +-- ProgramRegistry
              ProgramModule[] + capability predicates
```

Board profile должен описывать доступные компоненты и wiring, но не создавать драйверы. Composition root конкретной
цели выбирает provider-ы, связывает зависимости и запускает lifecycle.

## Этап 0. Закрыть архитектурный долг

Открытых пунктов этапа 0 не осталось.

Baseline от 16 июня 2026:

| Target         | Preset                  |   text |  data |    bss |    dec |                                                Image |
|----------------|-------------------------|-------:|------:|-------:|-------:|-----------------------------------------------------:|
| Pico           | `lexxos-pico`           | 358052 |     0 | 177744 | 535796 | `lexxos.uf2` 708096 bytes, `lexxos.bin` 353956 bytes |
| Pico full      | `lexxos-pico-full`      | 406252 |     0 | 179024 | 585276 | `lexxos.uf2` 804352 bytes, `lexxos.bin` 402156 bytes |
| STM32F407      | `lexxos-stm32f407`      | 301804 | 39388 |  97336 | 438528 |                            `lexxos.bin` 341244 bytes |
| STM32F407 full | `lexxos-stm32f407-full` | 459796 | 39388 | 109496 | 608680 |                            `lexxos.bin` 499236 bytes |

Simulator baseline:

- `cmake --build --preset lexxos-sim`: ok, `build/sim/src/liblexxos_sim.dylib` is `arm64` on current macOS host.
- `cmake --build --preset lexxos-sim-full`: ok, `FEATURE_GAMEBOY: ON`, `build/sim-full/src/liblexxos_sim.dylib` is
  `arm64` on current macOS host.
- Standard stable subset:
  `PYTHONPATH=emulator python3 -m pytest emulator/tests/test_i2c.py emulator/tests/test_run.py emulator/tests/test_sd.py emulator/tests/test_shell.py emulator/tests/test_virtual_board.py --lexxos-sim-lib build/sim/src/liblexxos_sim.dylib`
    - `24 passed`, `real 10.34s`.
- Full regression:
  `PYTHONPATH=emulator python3 -m pytest emulator/tests --lexxos-sim-lib build/sim-full/src/liblexxos_sim.dylib`
    - `46 passed, 1 skipped`, `real 19.68s`.

## Этап 1. Явный composition root

Открытых пунктов этапа 1 не осталось. Следующие работы переходят в этапы 2-4: platform service contracts, driver
providers и экземпляр VFS.

## Этап 2. Контракты platform services

Сделано:

- `PlatformServices` покрывает lifecycle, board, console, clock, GPIO, power и именованные flash regions;
- `Runtime` передаёт `PlatformServices` в `OperatingSystem`;
- `OperatingSystem` и `AppContext` позволяют держать разные platform services в разных экземплярах OS;
- Ctrl+C polling, shell task loop, terminal input, `uptime`, `led` и `reboot` переведены на services через composition;
- app-level clock calls в device live tests, storage benchmark/stress/speed-scan и Game Boy runner переведены на
  `ClockService` через `AppContext`;
- debug status display, system event SD detect/logging и SD automount logging используют `PlatformServices` текущего
  runtime/OS instance;
- `Shell`, `AppContext` default stdout/stderr и `ConsoleOutputStream` используют console service текущего OS instance;
- matrix keyboard, PCF8575 keyboard, Sega gamepad и M5Stack buttons drivers принимают `PlatformServices` для clock/GPIO;
- VFS pseudo files `/proc/uptime` и `/sys/board` получают clock/board через `vfs::initialize(PlatformServices)`;
- Game Boy cartridge cache metrics/retry sleeps и `gameboy-rom-cache` flash access идут через `PlatformServices`;
- `SdSpiBackend` принимает `PlatformServices` для retry sleeps, card-detect GPIO и diagnostic timestamps;
- `SdSdioBackend`, `sd_card` entrypoint, `SdAutomountService`, storage commands и simulator SD backend принимают
  `PlatformServices` для SD probe/retry sleep, board interface lookup и card-detect GPIO;
- runtime composition layer передаёт `PlatformServices` в board-provider layer, который связывает SD block device через
  services текущего `Runtime`;
- `St7735Display`, `M5StackLcdDisplay`, `Stm32ParallelTftDisplay`, STM32 ILI9341 init sequence и debug OLED display
  принимают `PlatformServices` для GPIO, delay и board wiring;
- board-provider layer использует board descriptor текущего OS instance при создании display, keyboard/gamepad и ESP-IDF
  M5Stack devices;
- `SchedulerBackend` привязывается к `PlatformServices` текущего `Runtime`, а Pico/bootstrap scheduler, simulator
  scheduler,
  FreeRTOS scheduler stop log и FreeRTOS panic hooks используют этот backend вместо прямых `platform::*`;
- старые `platform::*` free functions остались compatibility fallback-ом внутри services/backend adapters и ранних panic
  paths;
- `cmake/build_info.h.in` знает `LEXXOS_PLATFORM_ESPIDF`, поэтому команда `version` больше не покажет `unknown` на
  ESP32;
- host contract test проверяет instance-scoped services и stdout routing;
- services contract test покрывает fake services для shell, storage/block device command и Game Boy cartridge
  clock/flash сценариев.

Осталось:

- по этапу 2 ничего не осталось.

Критерий: storage, shell и Game Boy core не включают platform-specific headers и тестируются с fake services.

## Этап 3. Board manifest и driver providers

Заменить растущий `BoardDescriptor` на две части:

- базовая идентичность платы и processor family;
- список component declarations с type id, instance name, wiring и properties.

Пример концепции:

```text
spi.bus     name=spi0
sd.spi      name=sd0 bus=spi0 cs=17 detect=22
display     driver=st7735 bus=spi0 width=128 height=160
i2c.bus     name=debug
input       driver=pcf8575-matrix bus=debug address=0x20
```

Каждый `DriverProvider`:

- проверяет, поддерживает ли declaration;
- создаёт или предоставляет статически выделенный driver instance;
- регистрирует устройство;
- возвращает lifecycle hooks и диагностический статус.

Никакого runtime string parsing на MCU не требуется: manifest может быть `constexpr` массивом типизированных структур.

Уже зафиксированное правило для composite providers: один provider может регистрировать несколько публичных contracts
для одного hardware controller, если controller имеет общий state или destructive registers. Первый рабочий пример —
PicoCalc STM32 southbridge: provider `picocalc-keyboard` владеет FIFO, регистрирует `InputDevice` и одновременно
экспортирует `picocalc-southbridge` runtime service через facade для cached battery/backlight/power операций. Текущий
runtime composition root сохраняет это как единый provider-owned controller, а не как отдельный raw I2C driver для shell.

Критерий: добавление нового дисплея не требует изменения `DeviceManager`.

## Этап 4. Экземпляр VFS и filesystem registry

Статус 0.0.7: основной переход выполнен. `lexxos::vfs::Vfs` владеет mount table, block-device namespace, tmpfs,
pseudo-node state и per-instance filesystem factory registry. `OperatingSystem::vfs()`/`AppContext::vfs()` являются
явными точками доступа, а free functions сохранены как compatibility adapter над `defaultVfs()`.

Преобразовать глобальный VFS API в объект `Vfs`, содержащий:

- mount table;
- block-device namespace;
- tmpfs;
- pseudo-node providers;
- filesystem factory registry.

Filesystem factory должна создавать/mount-ить backend по имени:

```text
fat
tmpfs
```

FAT получает `BlockDevice&` и mount context напрямую, а не ищет устройство через глобальный VFS.

Для миграции можно временно оставить free functions как adapter над default `Vfs`.

Критерий выполнен на уровне tmpfs/factory registry: contract test создаёт два изолированных `Vfs` instance и проверяет,
что `/tmp` state и factory registrations не протекают между ними. Следующий возможный шаг — углубить FatFs volume-slot
ownership, если понадобится несколько независимых FAT pools в одном процессе.

## Этап 5. Модульная регистрация программ

Ввести `ProgramModule`:

```cpp
struct ProgramModule {
    std::string_view name;
    Span<ProgramDescriptor const> programs;
    CapabilityPredicate available;
};
```

Composition root передаёт список модулей в `ProgramRegistry`. Feature-флаги выбирают модули на уровне target, а не
внутри центрального `registerBuiltinPrograms()`.

Документация команд должна продолжать генерироваться из тех же descriptors/runtime help.

Критерий: новый набор команд добавляется одним module source list и одной записью target composition.

## Этап 6. Lifecycle и системные события

Статус 0.0.7: введён `lexxos::lifecycle::EventBus` с фиксированной очередью и cursor-подписчиками. `SystemEventService`
читает события из bus; SD card-detect ISR публикует компактное `SdCardDetectChanged` и ставит fallback-флаг, а
debounce/automount остаются в task context. `program_input::publish()` дополнительно публикует `Input`, а `poweroff`
публикует
`PowerOffRequested`/`ShutdownRequested`.

Добавить единый контракт lifecycle:

- `initialize`;
- `start`;
- `poll` или event subscription;
- `stop`;
- `status`.

`SystemEventService` преобразовать в небольшой event bus с фиксированной очередью. SD hot-plug, input и будущие power
events публикуют типизированные события; automount и UI подписываются без прямой связи друг с другом.

ISR по-прежнему только публикует компактное событие или ставит fallback-флаг, тяжёлая работа выполняется в task context.

Критерий частично закрыт: event bus покрыт contract tests, SD обработка сохраняет fake-friendly task-context границу.
Полный isolated hot-plug service test с fake clock/fake block device можно добавить после выделения debounce policy из
`SystemEventService`.

## Этап 7. Profiles, capabilities и конфигурация памяти

Статус 0.0.7: введены `lexxos::runtime::RuntimeProfile`, `standardRuntimeProfile` и
`lexxos::runtime::CapabilitySet`. В профиль вынесены ёмкости `ProgramManager`, process table, supervisor arguments,
shell buffers/history, `DeviceManager`, VFS/mount/tmpfs/factory limits, lifecycle/program-input queues и stack sizes
системных runtime tasks. `OperatingSystem::runtimeProfile()` возвращает активный профиль, а contract tests проверяют,
что публичные лимиты модулей читаются из этого источника.

Закрыто для `standard` profile:

- количество программ и устройств;
- mount slots;
- tmpfs limits;
- event queue size;
- task stack sizes;
- optional workspaces.

`OperatingSystem::refreshCapabilities()` собирает runtime capabilities из `DeviceManager`, board descriptor и
`PlatformServices`; `AppContext::capabilities()` передаёт snapshot приложениям. Переносимый runtime уже проверяет
capability для console mirror, local input task, SD automount/hot-plug, PicoCalc boot/backlight restore и Wi-Fi
autoconnect:

```text
display.rgb565
storage.block
storage.removable
input.keyboard
input.gamepad
flash.named-regions
```

Compile-time flags остаются инструментом исключения кода из firmware, но не основным API взаимодействия модулей.

Остаётся: board/target выбирает профиль `minimal`, `standard`, `full` или специализированный Game Boy profile.

## Этап 8. Статические и внешние модули

После стабилизации contracts:

- MCU: статически скомпонованные modules из constexpr registry;
- host/simulator: optional shared-library modules для экспериментов;
- VFS: manifest внешней программы с версией API, requirements и entry point;
- capability-based sandbox для будущих external programs.

Этот этап нельзя начинать до удаления singleton/global state из модулей, иначе unload, изоляция и тестирование будут
ненадёжными.

## Порядок реализации

Рекомендуемые инкременты:

1. Завершить миграцию call sites на `PlatformServices`.
2. `Vfs` как экземпляр.
3. Driver providers для simulator, затем Pico и STM32.
4. Program modules.
5. Event bus и lifecycle.
6. Runtime profiles/capabilities.
7. Optional host plugins и external program research.

Каждый инкремент должен сохранять старый adapter до миграции всех call sites. Нельзя одновременно переписывать
composition, VFS и device creation одним патчем.

## Подготовленная модель: отмена программ и арбитраж устройств

Эти прикладные изменения должны ложиться поверх текущей архитектуры и не блокировать миграцию composition root.

### Ctrl+C

В этой версии вводится кооперативная отмена программы:

- shell обрабатывает байт `0x03` как Ctrl+C и сбрасывает текущую строку ввода;
- `ProgramStartRequest` передаёт `CancellationToken` в дочерний `AppContext`;
- долгие программы проверяют `context.pollCancellationRequest()` и завершаются с кодом `130`;
- интерактивные программы, которые читают клавиши через `terminal::readKey()`, продолжают получать `KeyCode::CtrlC`.

Жёсткое убийство произвольного C++ handler-а без отдельной задачи не является безопасной операцией на MCU. Поэтому
текущая гарантия: программа завершается на ближайшей кооперативной точке проверки. В будущем тот же токен станет
сигналом от console supervisor к отдельной program task.

### Арбитраж внешних устройств

Рабочая модель доступа к устройствам:

1. Физические ресурсы имеют короткий atomic claim: I2C bus, block device, display, input device, gamepad.
2. Claim содержит `owner` и `priority`, чтобы диагностика показывала, кто держит ресурс.
3. Приоритеты задают политику планирования, но не прерывают уже начатую транзакцию:
   `Critical > System > Interactive > Normal > Background`.
4. Драйверы, которым нужна неделимая последовательность, используют виртуальную транзакцию поверх физической шины.
   Например, PCF8575 держит I2C transaction на всё время скана клавиатуры или двухфазного опроса Sega gamepad.
5. Дисплей арендуется на время кадра/clear/blit. Если ресурс занят, вторая программа получает понятную ошибку
   `display device is busy`, а не ломает текущий кадр.

Для PCF8575 это означает: клавиатура, Sega gamepad и будущие устройства на том же expander-е не должны выполнять
read-modify-write без общей transaction. Для GPIO через expander вводится правило: операции над группой пинов должны
идти через виртуальный интерфейс устройства, а не через независимые прямые записи отдельных клиентов.

Для SPI вводится тот же контракт: `SpiBusTransaction` защищает смену режима, CS и поток данных, а Pico SPI удерживает
mutex на время активной DMA-передачи от `startDmaTransfer()` до `waitDmaTransfer()`. SD-карта остаётся защищённой
верхнеуровневым block-device lease, но сама шина уже готова к появлению второго SPI-клиента.

Фоновые сервисы тоже должны быть видны в этой модели. Статусный debug OLED берёт `debug` I2C lease как
`Background/debug-status` и пропускает refresh, если шина занята интерактивной командой или системной операцией.

### База для program tasks и нескольких ядер

Отдельный запуск программ выносится в будущий инкремент. Подготовленная модель:

- `ProgramRunner` создаёт program task с фиксированным stack profile и передаёт туда `AppContext`;
- console task остаётся supervisor-ом: читает stdin, доставляет байты в очередь программы и посылает Ctrl+C в
  `CancellationToken`;
- stdout/stderr остаются stream-интерфейсами, поэтому их можно завернуть в потокобезопасный console writer;
- `TaskOptions` уже содержит диагностические `coreAffinity`/`executionClass`; будущий runner сможет использовать их
  для реального закрепления задач на ядрах, если целевая платформа это поддерживает;
- на одноядерных MCU runner работает как обычная FreeRTOS task, на RP2040/RP2350 можно закреплять тяжёлые программы
  на втором ядре только после явного lifecycle для драйверов и ресурсов.

Критерий будущего инкремента: shell не блокируется во время работы программы, Ctrl+C доставляется без участия самой
программы в чтении консоли, а все обращения к внешним устройствам проходят через resource lease или driver transaction.

## Проверки

После каждого этапа:

- host/simulator standard и full build;
- Pico standard/full build и размер firmware;
- STM32F407 standard/full build и SRAM/flash usage;
- Python pytest;
- новые C++ unit tests;
- hardware smoke затронутой подсистемы;
- проверка отсутствия новых platform macros вне composition/provider слоя.

## Критерии целевого состояния

- `Runtime` получает зависимости явно;
- singleton OS и global VFS больше не используются переносимым кодом;
- `DeviceManager` не создаёт конкретные platform drivers;
- board profile не содержит controller-specific orchestration;
- filesystem и program modules регистрируются через contracts;
- capability checks заменяют прямые platform checks в apps;
- simulator может собрать две независимые system composition в одном процессе;
- добавление нового board profile не требует изменения core, VFS, shell и существующих drivers.
