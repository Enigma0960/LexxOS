# `top`

watch memory, tasks and supervised jobs.

## Что показывает

`top` сейчас намеренно ориентирован на память и задачи, а не на CPU load:

- `internal heap` — текущий свободный heap и минимальный когда-либо свободный heap;
- `task stacks` — суммарный размер стеков задач, использованный объём и свободный high-water остаток;
- `program arena internal` — internal SRAM, зарезервированная `ProgramSupervisor` для рабочих областей программ;
- `psram` — состояние feature-флага, наличие wiring в board descriptor и external reserved/quota по supervised jobs;
- `TASKS` — задачи ядра, priority, class, core affinity, started, stack/used/free/use;
- `JOBS` — процессы `ProgramSupervisor`, их stack и internal/external memory quota.

CPU-проценты не выводятся, потому что текущий scheduler API не содержит runtime counters. Колонка `core` показывает только доступную сейчас информацию о core affinity задачи: конкретный номер ядра или `any`.

## Примеры

Один снимок без очистки экрана:

```text
top --count 1 --no-clear
```

Обновление раз в 2 секунды:

```text
top --interval 2000
```

Без таблицы supervised jobs:

```text
top --no-jobs
```

## Справка shell

```text
top - watch memory, tasks and supervised jobs

Periodically refreshes a compact memory-first dashboard with internal heap, task stack usage, PSRAM reservation accounting and ProgramSupervisor jobs. It intentionally does not report CPU percent until the scheduler exposes runtime counters.

Usage:
  top [--interval <ms>] [--count <samples>] [--no-clear] [--no-jobs]

Arguments:
  -i, --interval <ms> default=1000 - refresh interval in milliseconds, clamped to 100..60000
  -n, --count <samples> default=0 - number of refreshes; 0 means run until cancelled
  --no-clear - append snapshots instead of clearing the terminal before every refresh
  --no-jobs - hide ProgramSupervisor job memory table

Runtime:
  launch: foreground-task
  stack: 4096 bytes
  priority: 1
  memory: internal-only
  interface: console
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
