# `console-info`

## Исходник

`src/lexxos/apps/devices/console_info_program.cpp`

## Подробное описание

Печатает состояние LCD console: геометрию, cursor, status bar, scrollback, горизонтальный viewport и аппаратный scroll state.
Команда помогает проверять standalone shell/editor поведение без внешнего UART.

## Параметры

Команда не принимает пользовательских параметров.

## Поля, связанные с viewport

- `size` — видимая геометрия LCD console в символах;
- `logical columns` — логическая ширина строки, сейчас до 256 символов;
- `view left` — текущий горизонтальный сдвиг и максимально допустимый сдвиг; в live-view может быть ненулевым из-за
  автоматического слежения за курсором в длинной строке;
- `view top` — текущий вертикальный сдвиг по scrollback;
- `view` — live/history режим ручного просмотра scrollback/горизонтального viewport.

## Статус-бар

- `status battery` показывает текущий процент, если runtime смог прочитать батарею;
- на PicoCalc процент и charging flag приходят из southbridge snapshot;
- на Cardputer/Cardputer-Adv процент приходит из ADC-оценки напряжения, charging flag остаётся `no`, потому что
  аппаратный статус зарядки недоступен.

## Горячие клавиши PicoCalc/Cardputer

- `Shift/Ctrl + Up/Down` — вертикальная прокрутка на одну строку;
- `Alt/Sym + Up/Down` — вертикальная прокрутка страницей;
- `Shift/Ctrl + Left/Right` — горизонтальная прокрутка на несколько символов;
- `Alt/Sym + Left/Right` — горизонтальная прокрутка страницей;
- `Shift/Ctrl + Home/End` — в начало/конец строки по горизонтали;
- `Alt/Sym + Home/End` — в начало/конец scrollback по вертикали;
- `Esc` или обычный ввод — возврат к live-view и левому краю.

## Примеры

```text
lexx:/> console-info
```

## Комментарий автора

Используйте `console-info` после проверки status bar, RTC/battery snapshot, вертикального scrollback и горизонтального viewport, чтобы зафиксировать фактическое состояние консоли в smoke-test.

## Справка shell

```text
console-info - show LCD console state

Shows the current LCD console geometry, cursor, status bar, scrollback and hardware scroll state.

Usage:
  console-info
```

## Доступность

Доступна в стандартной или full-сборке в зависимости от feature-флагов платы. Документ соответствует LexxOS 0.0.7.
