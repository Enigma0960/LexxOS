# `gbemu-config`

show or initialize Game Boy emulator settings.

## Назначение

Команда показывает или создаёт единый файл настроек `gbemu`, который использует прямой запуск и TUI ROM launcher.

Стандартный путь:

```text
/mnt/sd/apps/gbemu/settings.ini
```

Базовый сценарий:

```text
gbemu-config --init
cat /mnt/sd/apps/gbemu/settings.ini
```

Для просмотра шаблона без записи:

```text
gbemu-config --show-defaults
```

В target-specific defaults есть секция `[audio]`. На Cardputer-Adv она печатается как `enabled = true`, чтобы TUI
launcher запускал выбранный ROM со звуком; прямой shell-запуск по-прежнему требует `gbemu --audio <rom>`.

Для принудительного восстановления defaults:

```text
gbemu-config --force
```

## Справка shell

```text
gbemu-config - show or initialize Game Boy emulator settings

Creates the default gbemu settings file used by the screen-oriented ROM launcher and prints the same defaults for manual editing.

Usage:
  gbemu-config [--path <path>] [--init] [--force] [--show-defaults]

Arguments:
  --path <path> default=/mnt/sd/apps/gbemu/settings.ini - settings file path; default is /mnt/sd/apps/gbemu/settings.ini
  --init - create the settings file if it is missing
  --force - overwrite an existing settings file with defaults
  --show-defaults - print default settings without writing a file
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
