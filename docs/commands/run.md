# `run`

## Исходник

`src/lexxos/apps/core/run_program.cpp`

## Подробное описание

Запускает программу через экспериментальный `ProgramSupervisor` task path. Foreground-запуск ждёт завершения
дочерней task, а background-запуск возвращает shell сразу и показывает process id для `jobs`/`stop`.

## Параметры

- `--bg` / `-b` — запустить программу как background job, если её `ProgramRuntimeProfile` разрешает это.
- `--console` / `-c` — оставить stdout/stderr background job в текущей консоли.
- `--quiet` / `-q` — перенаправить stdout/stderr background job в `NullOutputStream`; это поведение по умолчанию.
- `<program> [args...]` — имя программы и её аргументы; передаются дочернему `ProgramStartRequest`.

## Примеры

```text
lexx:/> run board
lexx:/> run edit --help
lexx:/> run sd-bench 1
lexx:/> run --bg sd-bench --delay-ms 10 1024
```

## Комментарий автора

Обычный shell-dispatch пока остаётся основным путём. `run` нужен для проверки stack high-water mark, cooperative
cancellation и будущей модели параллельных задач до включения supervisor по умолчанию.

## Справка shell

```text
run - run a program through ProgramSupervisor

Starts a program through the experimental ProgramSupervisor task path. Background jobs are quiet by default.

Usage:
  run [--bg] [--console] <program> [args...]
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
