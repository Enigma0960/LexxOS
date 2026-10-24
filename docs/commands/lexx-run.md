# `lexx-run`

## Исходник

`src/lexxos/apps/lexx/lexx_run_program.cpp`

## Подробное описание

Запускает внешний исполняемый файл `.lexx` из VFS/SD через экспериментальный RV32I runtime. Команда валидирует
заголовок LEXX, выделяет ограниченную guest-память, передает guest-программе `argc/argv` и выполняет системные
вызовы текстовой консоли, клавиатуры и read-only файлового доступа.

## Параметры

- `--trace` - печатать PC и инструкцию перед каждым шагом RV32I-интерпретатора.
- `--max-steps <count>` - ограничить число исполненных инструкций; `0` означает запуск без лимита.
- `<file.lexx> [args...]` - путь к внешней программе и аргументы, которые guest увидит как `argv`.
- `--` - завершить разбор опций `lexx-run`, если путь к файлу начинается с дефиса.

## Примеры

```text
lexx:/> lexx-run /mnt/sd/ext/hello.lexx alpha beta
lexx:/> lexx-run --trace /mnt/sd/ext/hello.lexx
lexx:/> lexx-run --max-steps 500000 /mnt/sd/ext/hello.lexx
```

## Комментарий автора

Прямой запуск `/mnt/sd/ext/app.lexx` из shell остается основным пользовательским путем; `lexx-run` нужен для
явной диагностики runtime и лимитов.

## Справка shell

```text
lexx-run - run a .lexx RV32I program

Loads a LexxOS external executable from VFS, validates its LEXX header, allocates a bounded guest memory arena and executes RV32I code through the text-mode ABI.

Usage:
  lexx-run [--trace] [--max-steps <count>] <file.lexx> [args...]

Runtime:
  launch: foreground-task
  stack: 8192 bytes
  priority: 1
  memory: external-preferred
  interface: console
  external quota: 65536 bytes
  internal workspace: 65536 bytes
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
