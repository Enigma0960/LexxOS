# Runtime-профиль программ LexxOS

Документ описывает текущую модель запуска программ через `ProgramRuntimeProfile` и `ProgramSupervisor`.
Цель этой работы — постепенно перейти от прямого синхронного вызова program handler к управляемым процессам FreeRTOS,
при этом держать стек task-а минимальным, а крупные данные переносить в arena/PSRAM.

Текущие измерения stack high-water mark на PicoCalc вынесены в
[`program-stack-audit.md`](program-stack-audit.md).

## Цель

Основная цель — уменьшить обязательную внутреннюю SRAM, выделяемую на каждый task, и перенести крупные рабочие данные
программ во внешнюю память, прежде всего PSRAM на PicoCalc.

Task должен хранить только небольшой стек исполнения, состояние вызовов и локальные переменные малого размера. Буферы,
ROM cache, документы, scrollback и другие крупные данные должны выделяться через будущий memory arena слой.

Практическое правило:

```text
маленький стек task-а      -> internal SRAM
рабочие данные программы   -> arena / heap / PSRAM / cache
долговременные данные      -> VFS / SD
```

## Runtime profile

Каждая программа может задать `ProgramRuntimeProfile` в `ProgramDescriptor`.

### Launch mode

```text
Inline
ForegroundTask
BackgroundTask
```

`Inline` означает короткую команду, которая может выполняться в текущем shell task. Это подходит для `help`, `pwd`,
`version`, `board`, `mem`, `tasks` и похожих команд.

`ForegroundTask` означает интерактивную или долгую программу, которую нужно запускать отдельной FreeRTOS task, но с
занятым
foreground console ownership.

`BackgroundTask` зарезервирован для программ, которые могут работать без интерактивного ввода и без владения консолью.
Фактический background-запуск включается отдельно через `allowBackground`.

### Memory class

```text
InternalOnly
ExternalPreferred
ExternalRequired
```

`InternalOnly` означает, что программе не требуется внешняя PSRAM.

`ExternalPreferred` означает, что программа может работать без PSRAM, но при её наличии должна переносить крупные буферы
во внешнюю память.

`ExternalRequired` означает, что программа должна получить внешнюю память для нормальной работы на целевом профиле
платы.

## Начальные профили

Текущие профили подобраны консервативно и проверяются по `jobs`/stack high-water mark:

```text
append:    ForegroundTask, stack 4096, internal workspace, internal-only
cat:       ForegroundTask, stack 6144 on ESP-IDF, otherwise 4096, internal workspace, internal-only
cp:        ForegroundTask, stack 4096, internal workspace, internal-only
edit:      ForegroundTask, stack 4096, internal workspace, external-preferred, quota 128 KiB
fs-smoke:  ForegroundTask, stack 5120, internal workspace, internal-only
gbemu:     ForegroundTask, stack 20480 on ESP-IDF, otherwise 12288, external-preferred on host/sim/ESP-IDF, external-required on PSRAM-oriented Pico profiles
hexdump:   ForegroundTask, stack 4096, internal workspace, internal-only
ls:        ForegroundTask, stack 8192 on ESP-IDF/Cardputer-Adv, otherwise 4096, internal workspace, internal-only
mic:       ForegroundTask, stack 10240, internal workspace, internal-only, cancellable
mkdir:     ForegroundTask, stack 4096, internal workspace, internal-only
mount:     ForegroundTask, stack 4096, internal workspace, internal-only
mv:        ForegroundTask, stack 4096, internal workspace, internal-only
psram-test: ForegroundTask, stack 4096, internal workspace, internal-only
rm:        ForegroundTask, stack 4096, internal workspace, internal-only
rmdir:     ForegroundTask, stack 4096, internal workspace, internal-only
sd-bench:  ForegroundTask, stack 5120, internal workspace, internal-only, allow-background
sd-stress: ForegroundTask, stack 5120, internal workspace, internal-only
sd-test:   ForegroundTask, stack 4096, internal workspace, internal-only
stat:      ForegroundTask, stack 4096, internal workspace, internal-only
storage:   ForegroundTask, stack 4096, internal workspace, internal-only
touch:     ForegroundTask, stack 4096, internal workspace, internal-only
umount:    ForegroundTask, stack 4096, internal workspace, internal-only
wifi-scan: ForegroundTask, stack 8192 on ESP-IDF/ESP-Hosted, 5120 on Pico/CYW43, internal-only
write:     ForegroundTask, stack 4096, internal workspace, internal-only
```

`write` и `append` используют общий text writer helper: path buffer и 96-byte stdin buffer выделяются в per-process
arena. При записи текста из аргументов stdin buffer всё равно резервируется, чтобы профиль оставался одним и тем же.

`cat` держит path buffer и 96-byte stream buffer в per-process arena. Стандартный ввод/вывод наследуется дочерним
foreground context, поэтому файловый и stdin-пути остаются тем же пользовательским интерфейсом.

`hexdump` держит path buffer и 16-byte row buffer в per-process arena. `ls` держит path buffer в arena.

`psram-test` держит три 256-byte диагностических буфера в internal arena. Сама команда пока остаётся
`InternalOnly`: она проверяет внешний PSRAM-чип, но не использует его как allocator backend.

`stat` держит path buffer в per-process arena.

`touch`, `rm`, `mkdir` и `rmdir` держат path buffer в per-process arena и выполняют VFS mutation в foreground task.

`mv`, `mount` и `umount` держат path buffers в per-process arena. `mount` без аргументов по-прежнему выводит текущую
таблицу mount points, но запускается через тот же foreground task profile для измеримости через `jobs`.

`cd` пока остаётся inline-командой: она меняет working directory текущего shell context, поэтому перенос в отдельный
task требует отдельной проверки семантики наследования и commit-а cwd обратно в shell.

`cp` держит source/target path buffers и 128-byte transfer buffer в per-process arena. Это убирает локальные VFS-буферы
из стека и делает прямой запуск `cp <source> <target>` supervised foreground job.

`fs-smoke` держит path buffers, read buffer и маленький copy buffer в per-process arena. Это оставляет стек task-а для
глубины VFS/FAT вызовов и делает обычный запуск `fs-smoke /mnt/sd` измеряемым через `jobs`.

`sd-bench` держит path buffer и общий 512-byte I/O buffer в per-process arena, чтобы foreground/background task stack
оставался ближе к фактической глубине вызовов VFS/SD.

`sd-test` держит path buffer и 96-byte readback buffer в per-process arena. `sd-stress` держит path buffer и два
512-byte FAT verification buffers в per-process arena, а длинный цикл проверяет cooperative cancellation.

`storage` держит 512-byte block preview/read buffer в per-process arena для веток `read0`, `read` и `diag`.

`wifi-scan` временно остаётся с глобальным scan state и увеличенным stack profile. CYW43 callback может пережить timeout
команды, поэтому перенос `WifiScanState` в per-process arena требует отдельного owner/cancel lifecycle.

## Правила для будущих программ

Новые программы должны соблюдать ограничения:

```text
- не размещать на стеке буферы больше 256 байт без явной причины;
- буферы больше 512 байт переносить в arena;
- долгие циклы должны проверять cancellation token;
- долгие циклы должны периодически вызывать sleep/yield;
- владение файлами и устройствами должно быть привязано к lifetime процесса.
```

## ProgramSupervisor

`ProgramSupervisor` — экспериментальный слой управления процессами. Обычный запуск из shell теперь выбирает путь по
runtime-профилю: `Inline`-команды остаются в shell task, а программы с task-профилем автоматически запускаются через
supervisor.

```text
board       -> inline shell/ProgramManager путь
edit file   -> supervisor foreground task
run board   -> явный supervisor-путь для проверки
```

Команды supervisor:

```text
run <program> [args...]
run --bg <program> [args...]
run --bg --console <program> [args...]
jobs
jobs clear
stop <id>
```

`ProgramSupervisor` сейчас делает:

```text
- выделяет process slot;
- копирует argv в память процесса;
- создаёт отдельный CancellationToken;
- создаёт дочерний AppContext;
- резервирует per-process `ProgramMemoryArena` по `ProgramRuntimeProfile::memoryClass`;
- запускает foreground/background task с параметрами из ProgramRuntimeProfile;
- сохраняет launch status, exit code и stack high-water mark;
- показывает процессы через jobs;
- очищает историю завершённых процессов через jobs clear;
- принимает cooperative stop request через stop <id>.
```

Текущий кодовый контур разложен на отдельные шаги:

```text
validateStartRequest -> prepareSlot -> reserveProcessMemory -> createProcessTask
```

`ProgramMemoryArena` уже резервирует небольшой internal workspace из heap по
`ProgramRuntimeProfile::internalMemoryQuotaBytes` и передаётся программе через `AppContext`. Это позволяет убирать
крупные состояния из static/global storage: `edit` теперь создаёт `EditorSession` в per-process arena.

Внешняя память пока остаётся policy gate без полного PSRAM allocator: `InternalOnly` запускается без внешней памяти,
`ExternalPreferred` продолжает работать с нулевой внешней квотой в сборках без `LEXXOS_FEATURE_PSRAM`, а
`ExternalRequired` регистрируется как disabled в таких сборках. CMake дополнительно печатает warning для включённых
feature-групп, где есть обязательные PSRAM-программы.

Обычный shell-запуск теперь учитывает `ProgramRuntimeProfile::launchMode`: программы с task-профилем запускаются через
`ProgramSupervisor` автоматически. Команда `run` остаётся явным интерфейсом для background-режима, вывода в console/null
и отладки таблицы процессов через `jobs`.

`jobs` показывает internal/external arena квоты и фактический резерв в колонках `int`/`ext`. При завершении процесса
arena отвязывает память от process slot под guard, а фактическое освобождение делает уже снаружи guard.

Глобальная таблица process slots сейчас защищена локальным guard внутри `program_supervisor.cpp`: на FreeRTOS это
`taskENTER_CRITICAL()`/`taskEXIT_CRITICAL()`, на host/sim — `std::mutex`, а bootstrap-сборка остаётся без lock, потому
что ещё не запускает параллельные task. Перед включением запуска из нескольких независимых task лучше вынести guard в
переносимый kernel primitive и описать правила ожидания/таймаутов.

## Foreground-запуск

Foreground-запуск выполняется через:

```text
run <program> [args...]
```

Shell ждёт завершения процесса и возвращает prompt после выхода программы. Это безопасный путь для проверки того,
сколько
стека реально требуется программе при запуске отдельной FreeRTOS task.

Для supervised task используются защитные минимумы:

```text
Inline через run:          минимум 4096 байт
Foreground/background:     минимум 4096 байт
ProgramRuntimeProfile:     может задавать больше
```

Минимумы supervisor, ёмкость process table, лимиты аргументов, дефолтный stack `ProgramRuntimeProfile` и дефолтные
optional workspace quotas берутся из `lexxos::runtime::standardRuntimeProfile.programs`. Командные descriptors
по-прежнему задают собственные stack/quota там, где конкретной программе нужно больше стандартного минимума.

## Background-запуск

Команда `run` поддерживает экспериментальный background-режим:

```text
run --bg <program> [args...]
run --bg --console <program> [args...]
jobs
stop <id>
```

Background-запуск разрешается только программам, у которых `ProgramRuntimeProfile::allowBackground = true`.
На текущем этапе таким тестовым сценарием является `sd-bench`, потому что он может работать без интерактивного ввода и
периодически проверяет `context.cancellationRequested()`.

По умолчанию stdout/stderr background-процесса перенаправляются в `NullOutputStream`, чтобы фоновый процесс не ломал
интерактивный ввод shell. Для ручной отладки можно вернуть вывод в консоль:

```text
run --bg --console sd-bench --delay-ms 10 128
```

Supervisor запрещает запуск второй активной копии той же программы. Это защищает программы вроде `sd-bench`, которые
используют общий временный файл (`/mnt/sd/LEXBENCH.TXT`) или общий аппаратный ресурс.

`stop <id>` является cooperative request. Процесс завершится только в точках, где программа проверяет cancellation
token.
Для проверки остановки `sd-bench` поддерживает искусственную задержку:

```text
run --bg sd-bench --delay-ms 10 1024
stop <id>
```

Ожидаемый exit code при cooperative cancellation — `130`.

## Проверенные результаты на PicoCalc Pico W

Тестовый профиль:

```text
board:    run board
mem:      run mem
sd-bench: run sd-bench 1
```

Результаты после уменьшения supervisor stack для inline-команд:

```text
board:    stack 4096, free 2056, used ~2040
mem:      stack 4096, free 2736, used ~1360
sd-bench: stack 6144, free 2376, used ~3768
```

Проверка background cancellation:

```text
run --bg sd-bench --delay-ms 10 1024
jobs
stop <id>
jobs
```

Подтверждённый результат:

```text
running -> stop requested -> finished, exit=130
```

## TODO / Roadmap

### 1. Закрепить job-модель

```text
- держать быстрые Inline-команды в shell task, если отдельный task им не нужен;
- продолжать проверять supervisor-путь через `run <program>`;
- расширить автоматический выбор supervisor-пути на новые ForegroundTask-программы;
- добавить явную политику console ownership для foreground-процессов;
- добавить вывод активных background job notifications без повреждения текущей строки ввода;
- добавить ограничение количества активных user jobs;
- добавить owner/resource lock для программ, работающих с общими файлами и устройствами;
- расширить `jobs` полями времени запуска, времени выполнения и причиной завершения;
- добавить `job-info <id>` или расширенный режим `jobs -v`.
- заменить локальный process table guard переносимым kernel mutex/critical-section API.
```

### 2. Развить ProgramMemoryArena

```text
- подключить platform PSRAM backend к существующему per-process ProgramMemoryArena;
- расширить small internal allocation для быстрых маленьких буферов;
- ввести external allocation для крупных рабочих буферов;
- привязать все выделения arena к lifetime job;
- освобождать все arena-выделения автоматически при завершении процесса;
- расширить quotas из ProgramRuntimeProfile до external allocator backend;
- расширить диагностику arena usage в `jobs`/`job-info`.
- отказ запуска с `ExternalRequired`, если PSRAM arena недоступна или quota не может быть выделена.
```

### 3. Подключить PSRAM-backed buffer/cache

```text
- вынести PSRAM read/write из диагностических программ в отдельный device/service;
- добавить mutex/resource lock для доступа к PSRAM;
- сделать блочный allocator поверх PSRAM;
- поддержать handle-based API: allocate/free/read/write;
- не отдавать программам raw pointer на PSRAM, пока PSRAM не memory-mapped;
- добавить `psram-map` для просмотра занятой внешней памяти;
- добавить `psram-bench` для оценки read/write throughput;
- добавить block/cache слой для последовательного чтения и записи;
- перенести крупные буферы `edit` в PSRAM-backed buffer;
- перенести ROM/cache/workspace части `gbemu` в PSRAM-backed arena;
- перенести большие временные буферы storage/fs tools в arena.
```

### 4. Уменьшить внутреннюю SRAM на процесс

```text
- собрать stack high-water mark для основных программ;
- оставить safety margin не меньше 1024 байт для стабильных программ;
- снижать stack только после переноса крупных буферов из локальных переменных;
- включить сборочные проверки `-fstack-usage` и/или `-Wframe-larger-than` для embedded preset;
- добавить debug-команду для просмотра минимального heap и максимального использования arena;
- отделить системные task-и от пользовательских jobs в диагностике.
```

### 5. Дальнейшие ограничения

```text
- не использовать `vTaskDelete(handle)` как штатный kill для чужого процесса;
- использовать cooperative cancellation через CancellationToken;
- интерактивные программы не запускать через `run --bg` без отдельного input/session layer;
- не считать PSRAM заменой обычного task stack/TCB на текущей PicoCalc реализации;
- хранить FreeRTOS task stack и TCB во внутренней SRAM.
```
