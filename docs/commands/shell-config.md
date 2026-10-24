# `shell-config`

## Исходник

`src/lexxos/apps/utility/shell_config_program.cpp`

## Подробное описание

Показывает и обновляет настройки shell line editor. Сейчас config хранит состояние fish-like autosuggestion и
историю ввода, которую shell лениво загружает из `/mnt/sd/.lexx/shell.conf` после появления SD mount.

## Параметры

- Без параметров / `show` — показать путь config, флаг загрузки, состояние autosuggest и размер истории.
- `load` — принудительно перечитать `/mnt/sd/.lexx/shell.conf`.
- `save` — записать текущие настройки и историю в `/mnt/sd/.lexx/shell.conf`.
- `autosuggest on|off` — включить или выключить серую подсказку из истории.

## Примеры

```text
lexx:/> shell-config show
lexx:/> shell-config autosuggest off
lexx:/> shell-config save
lexx:/> shell-config autosuggest on
```

## Комментарий автора

Autosuggest показывает только suffix самого свежего совпадения из истории, когда курсор стоит в конце строки.
Принять подсказку можно `Right`, `End` или `Ctrl-F`; `Left`, backspace, новый ввод или `Ctrl-C` очищают ghost text.

## Справка shell

```text
shell-config - show or update shell settings

Shows or updates shell autosuggestion settings and saves or reloads shell history.

Usage:
  shell-config [show|load|save|autosuggest on|off]
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
