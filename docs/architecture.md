# Архитектура LexxOS

Документ фиксирует текущее устройство проекта и ближайшее направление рефакторинга. Цель архитектуры — сохранить ядро,
VFS, shell и пользовательские программы переносимыми между host-сборкой, Raspberry Pi Pico, Pico 2 W, ESP32/ESP-IDF и
новыми платами.

## Основная идея

LexxOS строится вокруг строгого разделения переносимого кода и hardware-dependent кода.

```text
apps / shell / programs
        |
program manager / argument parser / stdio redirection
        |
OS context / kernel services / portable APIs
        |
VFS: paths, pseudo-files, mount points, file handles
        |
filesystem backends: tmpfs, FAT
        |
devices: display, storage, buses, sensors
        |
platform backend: startup, console, time, reboot
        |
board descriptor + processor family
```

Такой слой позволяет менять плату или SDK без переписывания прикладных программ.

## Правила зависимостей между слоями

1. Зависимости направлены сверху вниз. Нижний слой не должен включать headers верхнего слоя и не должен знать о
   командах, shell, пользовательском вводе или конкретном сценарии запуска.
2. Переносимый код не должен включать vendor SDK, FreeRTOS headers и platform-specific headers напрямую. Такие
   зависимости остаются в `src/lexxos/platform/*`, `src/lexxos/kernel/*` или platform-specific backend-файлах устройств.
3. `apps`, `shell` и `program` работают через `AppContext`, `ProgramManager`, VFS API и переносимые device contracts.
   Команда не должна создавать concrete driver и не должна обращаться к board profile для выполнения I/O.
4. `vfs` хранит правила путей, mount table, pseudo-files и file handles. VFS не должен знать о конкретной плате,
   SPI/I2C, GPIO, FreeRTOS task-ах или SD command protocol.
5. `fs` backend получает зависимости явно: `BlockDevice`, mount context и параметры cache/profile. Filesystem backend не
   должен самостоятельно искать устройство через global singleton или регистрировать себя через скрытый global state.
6. `devices` реализуют переносимые интерфейсы и concrete drivers. Driver может зависеть от `I2cBus`, `SpiBus`,
   `BlockDevice` и board wiring, но не от shell, программ или VFS paths.
7. `platform` предоставляет минимальные services: console, clock, GPIO, flash regions, reboot и scheduler binding.
   Platform backend не должен превращаться в registry конкретных внешних устройств.
8. Board profile описывает wiring и capabilities, но не выполняет orchestration. Создание drivers должно постепенно
   переехать в composition root и driver providers.
9. Compile-time feature flags допустимы для исключения кода из firmware. Внутри переносимого runtime предпочтительны
   capability checks и явно переданные services, чтобы feature flags не расползались по application code.
10. Временные compatibility adapters допустимы только на период миграции. Новый код должен получать зависимости явно и
    не добавлять новые обращения к singleton/global API.

Нарушение этих правил допустимо только как осознанный transitional debt, связанный с конкретным пунктом
[`docs/architecture-roadmap.md`](architecture-roadmap.md) или активного [`docs/todo/roadmap.md`](todo/roadmap.md).

## Базовые понятия

### Processor family

Processor family описывает семейство MCU или среду выполнения:

- `rp2040`;
- `rp2350`;
- `esp32`;
- `native-host`.

Эта сущность нужна для capability checks, low-level startup assumptions и будущего architecture-specific кода.

### Platform backend

Platform backend описывает слой интеграции с SDK/runtime:

- `host` — локальная desktop/host simulation;
- `pico` — Raspberry Pi Pico SDK backend;
- `espidf` — ESP-IDF backend для ESP32-плат, первый рабочий board profile: `m5stack_basic_v1`.

Platform backend должен оставаться небольшим. Его зона ответственности:

- ранняя инициализация runtime;
- UART/console backend;
- время и sleep/yield primitives;
- status LED;
- reboot/shutdown request;
- запуск scheduler backend.

Переносимый runtime получает эти операции через `lexxos::platform::PlatformServices` из
`src/lexxos/platform/platform_services.h`. Старые `platform::*` free functions остаются backend-adapter-ом на время
миграции existing call sites, но новый переносимый код должен зависеть от contracts, а не от процедурного platform API.

Конкретные устройства — display, SD card, keyboard, sensors, PSRAM, audio, wireless — должны жить в
`src/lexxos/devices/`, а не в `src/lexxos/platform/`.

### Board profile

Board profile описывает конкретную плату и pinout:

- `host_native`;
- `dev_pico`;
- `dev_pico2w`;
- `picocalc_pico_w`;
- `picocalc_pico2w`;
- `m5stack_basic_v1`.

Board descriptor объявляет wiring и capabilities: debug UART, status LED, OLED I2C, SD SPI, SD card-detect, PSRAM,
audio beep и wireless radio.
Descriptor не должен сам инициализировать драйвер устройства.

## Слой устройств

Device code находится в `src/lexxos/devices/`.

```text
src/lexxos/devices/
    bus/             переносимые I2C/SPI интерфейсы
    display/         display interfaces и controller drivers
    sensors/         общие интерфейсы датчиков
    storage/         block storage interfaces и SD/eMMC/flash-like media
    host/            host-side stubs/backends
    pico/            Pico SDK bus/device backends
    espidf/          ESP-IDF device backends for ESP32 boards
```

Текущий SSD1306 OLED driver зависит только от `I2cBus`. SPI SD-card driver зависит от `SpiBus` и общего
`BlockDevice`. Pico-specific реализации I2C/SPI изолированы в `devices/pico/`, а host-сборка использует stubs, чтобы
тот же application code собирался без физического железа.

При добавлении нового vendor SDK нужно добавлять device/backend файлы в отдельный backend-каталог вроде
`devices/pico/` или `devices/espidf/`, а не расширять platform code условиями под каждую периферию.

`DeviceManager` остаётся registry/lookup/lock facade и не создаёт concrete board drivers. Board-specific device creation
выполняется из runtime composition root через `composition/board_device_providers.cpp`, а общий lock/resource contract
живёт в `devices/device_resource.h` и `devices/detail/device_resource_registry.h`. Поэтому добавление нового provider
или display driver не требует изменений core registry.

### Runtime service contracts и composite ownership

Некоторые платы имеют composite controller, который выглядит как одно I/O-устройство, но обслуживает несколько
подсистем. Если часть регистров разрушает состояние при чтении или требует строгого порядка транзакций, такой controller
нельзя раздавать независимым драйверам через raw bus access.

Для таких случаев `DeviceManager` может регистрировать lockable runtime service рядом с обычными physical resources.
Service entry не заменяет `InputDevice`, `DisplayDevice` или `BlockDevice`; он фиксирует contract для операций, которые
должны идти через единственного runtime-владельца.

Текущий пример — PicoCalc STM32 southbridge:

- `PicoCalcKeyboard` является provider/runtime owner для southbridge FIFO;
- owner регистрируется как input device `picocalc-keyboard`, а отдельный runtime-service facade регистрируется как
  `picocalc-southbridge`;
- внутренний `PicoCalcKeyboardController` владеет controller state, diagnostics/recovery и runtime service facade, а
  публичный `PicoCalcKeyboard` остаётся тонким `InputDevice`;
- FIFO register читает только клавиатурный драйвер, потому что чтение забирает событие из очереди;
- `backlight`, `display-power`, `power`, `poweroff`, `picocalc-info` и `picocalc-sb` используют service contract,
  cached snapshot и service-backed commands;
- `PicoCalcSouthbridgeResourcePolicy` держит единое имя service и lock helper для приложений/runtime tasks, чтобы
  clients не дублировали resource id;
- direct I2C diagnostics допустимы только как bring-up путь без активного registered service.

При переносе device creation в будущие `DriverProvider` и composition root это правило должно сохраниться: provider,
создавший composite controller, регистрирует все публичные contracts для одного владельца, а runtime clients не обходят
его через raw register access.

## Kernel/runtime layer

Текущий runtime layer минимален и скрывает FreeRTOS/host threads за LexxOS API:

```text
kernel::createTask(...)
kernel::startScheduler()
kernel::sleepMs(...)
kernel::yield()
kernel::requestShutdown()
kernel::isShutdownRequested()
```

Host builds используют thread-based scheduler backend. Pico builds по умолчанию используют FreeRTOS backend.
Bootstrap fallback доступен через `LEXXOS_PICO_USE_FREERTOS=OFF`, но он нужен только для ранних CMake/bootstrap checks и
запускает только первую зарегистрированную задачу.

Application code не должен напрямую включать FreeRTOS headers.

## OS context

`OperatingSystem` и `AppContext` связывают основные подсистемы:

- active board descriptor;
- platform backend;
- runtime profile;
- device manager;
- VFS;
- tmpfs;
- program manager;
- shell context;
- runtime services.

Этот слой должен оставаться точкой сборки зависимостей, но не должен превращаться в место реализации команд,
filesystem operations или hardware polling.

## Runtime profile

`lexxos::runtime::RuntimeProfile` из `src/lexxos/runtime/runtime_profile.h` является единым источником фиксированных
лимитов portable runtime. Профиль сейчас представлен как `standardRuntimeProfile`: он `constexpr`, поэтому может
использоваться для размеров `std::array` без heap allocation и без дополнительной инициализации на MCU.

В профиль вынесены:

- ёмкость `ProgramManager`, process table, лимиты аргументов supervisor и дефолтные optional workspace quotas;
- лимиты shell: длина строки, число аргументов, history и injected input buffer;
- ёмкости `DeviceManager` для I2C, block/display/input/gamepad/audio и PicoCalc southbridge services;
- лимиты VFS: длина path/name, block-device namespace, mount slots, tmpfs и filesystem factories;
- размеры lifecycle/program-input event queues;
- stack sizes системных runtime tasks: console, system-events, console mirror, local console input, health и debug
  display.

`OperatingSystem::runtimeProfile()` возвращает активный профиль. На `0.0.7` все board profiles используют стандартный
набор лимитов; будущие `minimal`/`full`/Game Boy варианты должны появляться как profile selection, а не как новые
разрозненные константы в подсистемах.

## Runtime capabilities

`lexxos::runtime::CapabilitySet` из `src/lexxos/runtime/capabilities.h` является runtime snapshot возможностей текущего
`OperatingSystem`. Он собирается из двух источников:

- зарегистрированные contracts в `DeviceManager`: `display`, `display.rgb565`, `display.power-mode`, `storage.block`,
  `input.keyboard`, `input.gamepad`, `picocalc.southbridge`;
- board/platform contracts: `storage.removable`, `audio.tone`, `audio.pcm`, `wireless.wifi`, `wireless.bluetooth`,
  `flash.named-regions`, `memory.external.psram`, `gpio.user`, `power.reboot`.

`OperatingSystem::refreshCapabilities()` вызывается после регистрации board devices в composition root, а
`AppContext::capabilities()` отдаёт тот же snapshot приложениям. Команда `board` печатает строку `capabilities: ...`,
чтобы на устройстве можно было проверить, какие portable contracts видит runtime.

Compile-time feature flags остаются границей компоновки: они исключают исходники, драйверы и экспериментальные команды из
firmware. Внутри переносимого runtime feature flag больше не является главным API для orchestration: console mirror,
local keyboard input task, SD automount/hot-plug, PicoCalc boot finish/backlight restore и Wi-Fi autoconnect проверяют
capability текущего OS instance.

## Program layer

Пользовательские команды shell уже представлены как программы и регистрируются в `ProgramManager`.

После второго этапа `programs.cpp` стал точкой входа регистрации встроенных программ. Основной код программ
разнесён по тематическим каталогам:

```text
src/lexxos/apps/
    programs.h
    programs.cpp
    common/
        core_programs_support.*
        pretty_table.*
        program_registration.h
    core/
        system_programs.h
        system_programs.cpp
    fs/
        filesystem_programs.h
        filesystem_programs.cpp
    storage/
        storage_programs.h
        storage_programs.cpp
    utility/
        utility_programs.h
        utility_programs.cpp
    i2c_programs.h
    i2c_programs.cpp
```

Группы программ представлены регистрационными классами `SystemPrograms`, `StoragePrograms`, `FilesystemPrograms`,
`FatPrograms`, `UtilityPrograms` и `I2cPrograms`. Отдельные программы внутри групп оформлены как классы: объявления
находятся в `.h`, а реализации `descriptor()` и `run()` — в `.cpp`. Пользовательское поведение shell при этом не
меняется: имена команд, аргументы и вывод остаются прежними.

Табличный stdout встроенных shell-команд должен использовать `apps/common/pretty_table.*`. Этот helper печатает
ASCII-таблицы без Qt/STL stream formatting, держит один bounded row-buffer на стеке, ограничивает ширину ячеек и даёт
одинаковый текстовый вывод для UART, LCD mirror и emulator tests. Псевдо-файлы VFS могут оставаться простым
line-oriented text API, если их формат важен как машинно-читаемый интерфейс.

## VFS layer

VFS находится в `src/lexxos/vfs/`. Shell programs работают с путями и file handles, а не с внутренностями конкретного
filesystem backend.

Текущее дерево похоже на минимальную Linux-like структуру:

```text
/               virtual root
/dev/sd0        SD-card block device
/proc/uptime    runtime uptime pseudo-file
/proc/meminfo   heap statistics pseudo-file
/proc/tasks     registered task list pseudo-file
/sys/board      active board descriptor pseudo-file
/sys/storage    SD-card state pseudo-file
/mnt            mount root
/mnt/sd         default SD FAT mount point
/tmp            tmpfs
```

Поддерживаются:

- directory listing;
- metadata lookup;
- pseudo-files;
- file handles;
- read/write operations;
- mount/unmount;
- tmpfs;
- FAT32 mount через `BlockDevice`.

Начиная с подготовки 0.0.7 состояние VFS хранится в экземпляре `lexxos::vfs::Vfs`: mount table, `/dev` block-device
namespace, tmpfs, pseudo-nodes и filesystem factory registry больше не живут как отдельные process-global массивы.
`OperatingSystem::vfs()` и `AppContext::vfs()` являются основным путём доступа для runtime/composition code.
Старые free functions в `lexxos::vfs` оставлены как совместимый adapter над `defaultVfs()` на время постепенной миграции
команд и библиотек.

Filesystem factories регистрируются на конкретном `Vfs` instance. Текущий FatFs backend предоставляет factory aliases
`fat`, `fat16` и `fat32`, а также overload `fat::mount(Vfs&, source, target)`, поэтому mount path может получать
`BlockDevice` и mount context из выбранного экземпляра VFS. Contract tests создают два изолированных `Vfs` объекта и
проверяют, что tmpfs и factory registry не протекают между ними.

## FAT backend

Текущий FatFs backend поддерживает практическую работу с FAT32 SD-картой:

- superfloppy-style cards;
- первый поддерживаемый FAT partition в MBR;
- чтение файлов;
- создание файлов;
- write/append/truncate;
- copy/remove;
- mkdir/rmdir;
- mv/rename;
- ASCII long file names для файлов и каталогов;
- диагностику через `fs-info`, `fat-cache`, `sd-debug`, `sd-test`, `sd-bench`, `sd-stress` и `fs-smoke`.

Текущий production-профиль SD/FAT для `dev_pico` зафиксирован в `docs/sd-card-production.md`: SPI backend, 50 MHz
default, CMD18 для чтения, CMD25 выключен для записи, FAT read-ahead cache и стабильный write chunking через CMD24.

## SD automount и hot-plug

Текущая реализация использует общую системную задачу:

```text
system-events
```

Для `dev_pico` изменение уровня card-detect фиксируется аппаратным GPIO interrupt. ISR публикует компактное
`lifecycle::EventType::SdCardDetectChanged` событие в фиксированный `lifecycle::EventBus` и одновременно ставит atomic
fallback-флаг, чтобы hot-plug не зависел от ring-buffer overwrite во время плотного input потока. Debounce, console
output, SD probe, FAT mount/unmount и восстановление VFS выполняются позже в task context:

```text
GPIO IRQ -> lifecycle event -> system-events task -> debounce -> SdAutomountService
```

Если platform backend не поддерживает GPIO interrupt, `SystemEventService` использует polling fallback. Это сохраняет
работоспособность host-сборки и будущих плат, где card-detect может быть не подключён.

Нельзя выполнять VFS mount/unmount, SPI операции или console output непосредственно из interrupt handler.

Тот же `lifecycle::EventBus` принимает типизированные `Input`, `PowerOffRequested` и `ShutdownRequested` события.
Текущая специализированная очередь `program_input` остаётся основным интерфейсом foreground GUI/full-screen программ,
а lifecycle event даёт общий hook для будущих UI/power subscribers без связывания их с keyboard или poweroff command.

## Обновления через SD staging

Для плат с одновременно доступными `wireless.wifi` и `storage.removable` основной update pipeline должен быть
двухэтапным: network layer только скачивает payload во временный файл на VFS/SD, а installer работает уже с проверенным
локальным файлом. Это одинаково подходит для OTA firmware и для внешних `.lexx` программ.

Минимальный контракт такого updater-а:

- download пишет только в `.tmp` path внутри `/mnt/sd/.updates`;
- manifest хранит тип payload, версию, размер, hash и подпись;
- commit выполняется atomic rename после полной проверки файла;
- firmware installer ставит rollback marker до первой успешной загрузки;
- `.lexx` installer заменяет программы atomic rename-ом и оставляет предыдущую версию до успешного commit.

Такой pipeline не должен зависеть от конкретного транспорта: Wi-Fi, Ethernet, UART/YMODEM или USB mass-storage должны
давать один и тот же локальный staged artifact перед установкой.

## Почему это разделение важно

`dev_pico` и `picocalc_pico_w` используют один platform backend `pico`, но это разные board profiles. PicoCalc имеет
другой display, STM32 keyboard/southbridge controller, power-management и другой набор ограничений. ESP32 требует другой
platform backend. Если смешать эти понятия, код быстро станет RP2040-only и current-board-only.

## Следующее архитектурное обновление

Структурный рефакторинг 0.0.4 завершил разделение файлов и реализаций, но composition системы пока опирается на
singleton/global API и центральные feature-условия. Архитектурный контекст перехода к explicit composition root,
service contracts, driver providers, экземпляру VFS и модульной регистрации программ находится в
[`docs/architecture-roadmap.md`](architecture-roadmap.md). Активные задачи сведены в
[`docs/todo/roadmap.md`](todo/roadmap.md).

Изменения должны выполняться маленькими совместимыми патчами. Динамические plugins и external programs допустимы
только после стабилизации контрактов и удаления process-global state из переносимых модулей.
