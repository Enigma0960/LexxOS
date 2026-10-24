# PicoCalc LCD-консоль

Документ описывает экранную консоль LexxOS для профилей `picocalc_pico_w` и `picocalc_pico2w`.

## Назначение

LCD-консоль дублирует вывод shell на встроенный экран PicoCalc и позволяет использовать устройство без внешнего
UART-терминала. UART-консоль при этом сохраняется как основной диагностический канал для bring-up и аварийной отладки.

## Аппаратная конфигурация

Профиль платы разделяет настройки интерфейсов и устройств.

Интерфейсы:

```text
Console UART: UART0 GP0/GP1, 115200
System I2C:   I2C1 GP6/GP7, 400 kHz
Storage SPI:  SPI0 GP18/GP19/GP16
Display SPI:  SPI1 GP10/GP11
```

Устройства:

```text
Keyboard: PicoCalc STM32 southbridge, I2C address 0x1F
Display:  ILI9488-compatible LCD, 320x320, CS GP13, DC GP14, RESET GP15
SD card:  SPI SD, CS GP17, card-detect GP22 active-low
```

Важно сохранять это разделение: интерфейс описывает общие линии шины, устройство описывает только собственные сигналы
управления и параметры.

## Вывод текста

LCD-консоль использует символьную сетку 6x8 пикселей и RGB565 framebuffer-поток поверх display driver. Для ускорения
длинного вывода используется построчная отрисовка и аппаратный vertical scroll контроллера LCD.

Особенности текущей реализации:

- видимый курсор ввода отображается только в live-view;
- при вводе длинных строк live-view автоматически сдвигает горизонтальное окно так, чтобы курсор оставался на экране;
- при длинном выводе курсор временно скрывается и рисуется заново только в актуальной позиции;
- `clear` очищает видимый экран и возвращает view в live-tail;
- UART-вывод и LCD mirror получают один и тот же текст через `console_mirror`.

## Внутреннее устройство

LCD console разделена на маленькие detail-компоненты:

- `picocalc_lcd_console_layout` считает видимую геометрию и logical width из размеров display;
- `picocalc_lcd_console_terminal_state` владеет screen buffer, cursor state, ANSI/CSI parser и scrollback storage;
- `picocalc_lcd_console_scrollback_navigation` зажимает vertical/horizontal viewport и возвращает redraw/reset flags;
- `picocalc_lcd_console_text_renderer` рисует строки и ячейки из immutable render frame;
- `picocalc_lcd_console_renderer` остаётся coordinator-ом display lock, output queue, status bar, dirty rows и hardware
  scroll.

Такое разделение оставляет layout, scrollback navigation и terminal-state orchestration вне renderer-файла, но сохраняет
прежний публичный API `picocalc_lcd_console`.

## ANSI subset

LCD mirror не является полным terminal emulator, но обрабатывает VT/ANSI-последовательности, которые использует shell и
screen-oriented `edit`:

- `ESC[<row>;<col>H` / `ESC[<row>;<col>f` — абсолютная позиция курсора, координаты 1-based и зажимаются в видимую
  сетку LCD;
- `ESC[<n>A/B/C/D`, `ESC[<n>E/F`, `ESC[<col>G`, `ESC[<row>d` — перемещение курсора;
- `ESC[K`, `ESC[1K`, `ESC[2K` — очистка части или всей строки;
- `ESC[J`, `ESC[1J`, `ESC[2J`, `ESC[3J` — очистка части или всего видимого экрана;
- `ESC[?25l` / `ESC[?25h` — скрыть или показать курсор;
- `ESC[m`, `ESC[0m`, `ESC[30..37m`, `ESC[90..97m`, `ESC[40..47m`, `ESC[100..107m`, `ESC[39m`,
  `ESC[49m` — базовые SGR-атрибуты foreground/background через 16-цветную packed-палитру.

Этого достаточно, чтобы `edit` перерисовывал header, text area, status line и курсор на LCD так же, как на UART/USB
terminal. Более широкий ANSI subset нужно добавлять только под реальные приложения и проверять на PicoCalc hardware.

## Status Bar

Верхние 24 пикселя LCD зарезервированы под status bar, поэтому текстовая область console mirror начинается ниже этой
строки. Status bar показывает время слева, а справа заряд аккумулятора PicoCalc: процент заряда, пиктограмму батареи и
индикатор зарядки. Если RTC snapshot ещё недоступен, слева отображается `--:--:--` или компактное `--:--`; если battery
snapshot ещё недоступен, отображается `--%`.

Status bar рендерится отдельным компонентным layout: `Text`, `Icon`, `Separator` и `Spring`. `Spring` автоматически
забирает свободное место между левой и правой группой, поэтому проценты не могут наехать на battery icon. Battery icon
использует общий `Icon` component (`IconKind::Battery`), чтобы следующие индикаторы вроде Wi-Fi, Bluetooth или SD можно
было добавить без нового layout-кода.

Данные берутся из cached `picocalc-southbridge` snapshot через background task `picocalc-status` примерно раз в секунду.
Task использует service lock с background priority и не читает southbridge registers напрямую из shell path.

## Text/graphics handoff

LCD mirror работает в `ConsoleMirrorMode::Text`. Display diagnostics и будущие graphics apps могут временно взять
`ConsoleMirrorMode::Graphics` через `ScopedConsoleMirrorMode`: на это время PicoCalc LCD mirror перестаёт дорисовывать
текст поверх графического вывода, а после выхода из guard автоматически возвращается в text mode.

Текущие команды `display-fill`, `display-rect`, `display-bars`, `display-test` и `display-text` используют этот handoff
после успешного display resource lock. Сообщение об ошибке или успешном завершении печатается уже после восстановления
text mode, поэтому автономный shell снова видит ответ команды на LCD.

## Display power mode

Команда `display-power [on|off|sleep]` управляет режимом LCD controller через общий `DisplayDevice` API:

```text
display-power        показать текущий режим primary display
display-power off    выключить вывод контроллера
display-power sleep  перевести контроллер в sleep
display-power on     вернуть вывод после off/sleep
```

На PicoCalc `off` и `sleep` также гасят LCD и keyboard backlight через `picocalc-southbridge` service, а `on`
восстанавливает последние уровни. Если экран находится в `off` или `sleep`, первый `KeyDown` от PicoCalc keyboard
пробуждает display и не передаётся дальше в shell.

Автоотключение display настраивается той же командой:

```text
display-power timeout          показать текущий idle timeout
display-power timeout 300      выключать display через 300 секунд бездействия
display-power timeout 60 sleep переводить display в sleep через 60 секунд
display-power timeout off      отключить автоотключение
```

Настройки сохраняются в `/mnt/sd/.lexx/display-power.conf`. Пока config ещё не найден, используется runtime
default `300s` и режим `off`. Любой ввод в shell или `KeyDown` с PicoCalc keyboard сбрасывает idle timer.

Пока controller находится в `off` или `sleep`, pixel/scroll операции отвергаются драйвером, а pending LCD mirror output
сможет отрисоваться после `display-power on` или после пробуждения клавишей.

## Scrollback

Экранная консоль хранит ограниченную историю строк. История используется только для просмотра вывода и не меняет
состояние shell line editor.

Режимы просмотра:

```text
live     текущий вывод shell, курсор видим
history  ручной просмотр scrollback или горизонтальной позиции
```

При обычном вводе символов консоль возвращается в live-view. Автоматический горизонтальный сдвиг во время набора не
считается ручным history-режимом: `view left` может быть больше нуля, но `view` остаётся live, пока пользователь не
прокрутил экран модификаторами.

## Сочетания клавиш

Обычные стрелки сохраняют стандартное поведение shell:

```text
Up/Down     история команд shell
Left/Right  перемещение курсора внутри строки ввода
```

Scrollback управляется модификаторами:

```text
Shift + Up    прокрутка истории на одну строку вверх
Shift + Down  прокрутка истории на одну строку вниз
Ctrl + Up     прокрутка истории на одну строку вверх
Ctrl + Down   прокрутка истории на одну строку вниз
Sym + Up      прокрутка истории на страницу вверх
Sym + Down    прокрутка истории на страницу вниз
Alt + Up      прокрутка истории на страницу вверх
Alt + Down    прокрутка истории на страницу вниз
Sym + Home    перейти к началу доступной истории
Sym + End     вернуться в live-tail
Alt + Home    перейти к началу доступной истории
Alt + End     вернуться в live-tail
Escape        вернуться в live-view, если активен просмотр истории
```

Если scrollback не активен, `Escape` в PicoCalc shell mode работает как локальный cancel key и передаётся в shell как
`Ctrl-C`. Во время запущенной программы shell переключает console input в program mode, и `Escape` передаётся как
настоящий ESC для `terminal::readKey()`.

Обычные `Home`/`End` без `Sym`/`Alt` остаются terminal keys для shell line editor и приложений вроде `edit`.

## Диагностика

Для проверки состояния LCD-консоли используйте:

```text
console-info
```

Команда выводит геометрию консоли, позицию курсора, состояние status bar, scrollback, очередь вывода и параметры
hardware scroll.
Поле `text mode: no` означает, что LCD mirror временно остановлен graphics handoff, например во время display command.
Поле `cursor visible` показывает последнее состояние, заданное `ESC[?25l` / `ESC[?25h`.
Поле `status updated` показывает uptime timestamp последнего обновления status bar background task.

Базовый smoke-test после изменений:

```text
help
console-info
console-palette
shell-config show
echo autosuggest-color-history
type "echo auto" and accept the gray suffix with Right, End or Ctrl-F
check top status bar time, battery percent and status updated timestamp
Shift+Up / Shift+Down
Ctrl+Up / Ctrl+Down
Sym+Up / Sym+Down
Alt+Up / Alt+Down
Sym+Home / Sym+End
Alt+Home / Alt+End
Escape
clear
```

Документ соответствует LexxOS 0.0.7.
