# `jobs`

## Исходник

`src/lexxos/apps/core/jobs_program.cpp`

## Подробное описание

Показывает таблицу процессов `ProgramSupervisor`: id, состояние, имя программы, размер стека, свободный стек,
priority, memory class, внешнюю квоту arena, exit code и launch status.

## Параметры

- Без параметров — показать все активные и завершённые supervised processes.
- `clear` — удалить из таблицы завершённые процессы.

## Примеры

```text
lexx:/> jobs
lexx:/> jobs clear
```

## Комментарий автора

`jobs` сейчас является главным диагностическим окном для параллельной модели: по `stack free` подбирается
`ProgramRuntimeProfile::stackSizeBytes`, а поля `int`/`ext` показывают зарезервированные/запрошенные квоты arena.

## Справка shell

```text
jobs - show supervised program processes

Prints the ProgramSupervisor process table used by run/stop.

Usage:
  jobs [clear]
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
