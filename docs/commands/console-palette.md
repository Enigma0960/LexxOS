# `console-palette`

## Исходник

`src/lexxos/apps/utility/console_palette_program.cpp`

## Подробное описание

Печатает ANSI SGR образцы foreground/background цветов. UART-terminal получает обычные escape-последовательности,
а LCD console mirror разбирает тот же поток и рисует packed 16-color RGB565 palette.

## Параметры

Команда не принимает пользовательских параметров.

## Примеры

```text
lexx:/> console-palette
```

## Комментарий автора

Используйте эту команду как smoke-test после изменений terminal-state или renderer: цвета должны отличаться и на UART,
и на LCD mirror. Если UART terminal не показывает цвета, проверьте, что клиент не фильтрует ANSI SGR.

## Справка shell

```text
console-palette - show ANSI console palette

Prints ANSI SGR foreground and background color samples for serial terminals and LCD console mirror.

Usage:
  console-palette
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
