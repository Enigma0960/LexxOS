# `stop`

## Исходник

`src/lexxos/apps/core/stop_program.cpp`

## Подробное описание

Запрашивает cooperative cancellation для процесса, созданного `ProgramSupervisor`. Команда не убивает task
принудительно: процесс должен сам проверить `CancellationToken` и завершиться.

## Параметры

- `<id>` — обязательный id supervised process из вывода `jobs`.

## Примеры

```text
lexx:/> run --bg sd-bench --delay-ms 10 1024
lexx:/> jobs
lexx:/> stop 1
```

## Комментарий автора

Для LexxOS 0.0.7 это намеренно мягкая остановка. Штатный kill через `vTaskDelete(handle)` не используется, чтобы не
оставлять занятые ресурсы, файлы и будущие PSRAM arena allocations в неизвестном состоянии.

## Справка shell

```text
stop - request supervised process stop

Requests cooperative cancellation for a process created by ProgramSupervisor.

Usage:
  stop <id>

Arguments:
  id required - supervised process id
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
