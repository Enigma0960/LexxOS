# PicoCalc keyboard layout

Документ фиксирует реализованную и проверенную раскладку клавиатуры ClockworkPi PicoCalc для текущей STM32 firmware.
Дальнейшие cleanup-задачи вынесены в [`docs/todo/roadmap.md`](todo/roadmap.md).

## Слои физической клавиатуры

По разметке клавиатуры PicoCalc:

- базовый слой — белые подписи на клавишах;
- `Shift`-слой — зелёные подписи;
- `Alt`-слой — жёлтые подписи.

`Power` не нужно искать в keyboard FIFO на текущей проверенной прошивке. Отдельный PMU/power-event source пока не
планируется: автоматическое выключение экрана и подсветки закрывает текущий пользовательский сценарий.

## Реализованное покрытие

Mapping реализован в `src/lexxos/devices/input/picocalc_keyboard_layout.cpp`. Драйвер `picocalc_keyboard.cpp`
использует этот модуль для `scanCode + modifiers -> KeyCode/character/modifierMask`, а host contract test
`tests/cpp/picocalc_keyboard_layout_tests.cpp` фиксирует известные scan-code.

Проверено на реальном PicoCalc через `keyboard-live --full`:

- добавлены `KeyCode` для всех базовых латинских букв `A..Z`;
- добавлены `KeyCode` для основных ASCII punctuation keys: `Tab`, grave, minus, equals, brackets, backslash, semicolon,
  apostrophe, comma, period, slash;
- добавлены `KeyCode` для `F1..F10`, `Break`, `Home`, `End`, `Delete` и `Insert`;
- PicoCalc mapping покрывает базовые ASCII-коды и shifted punctuation variants;
- PicoCalc mapping больше не отбрасывает function-key/system-key scan codes как `Unknown` без `character`;
- фактический символ сохраняется в `InputEvent::character`;
- внутренняя маска pressed keys в PicoCalc keyboard расширена до 64 бит, чтобы новые `KeyCode` могли участвовать в
  `isPressed()`;
- `keyboard-live --full` включает диагностический raw-event reporting, где unmapped non-character события не
  отбрасываются, а печатаются как `unknown scan=0x.. mods=..`;
- проверенный набор Shift/system keys не даёт `unknown` событий.
- host contract tests покрывают printable mapping, system scan-code, modifier scan-code и repeatable-key policy.

## Наблюдённые scan-code PicoCalc

Таблица составлена по `keyboard-live --full` на реальном PicoCalc.

```text
F1             0x81
F2             0x82
F3             0x83
F4             0x84
F5             0x85
Shift+F1       0x86  -> F6
Shift+F2       0x87  -> F7
Shift+F3       0x88  -> F8
Shift+F4       0x89  -> F9
Shift+F5       0x90  -> F10
Shift+Esc      0xD0  -> Break
Alt+Ins        0xD1  -> Insert
Shift+Tab      0xD2  -> Home
Del            0xD4  -> Delete
Shift+Del      0xD5  -> End
```

`0x8A` оставлен как legacy/compatibility alias для `F10`, но проверенная STM32 firmware отдаёт `F10` как `0x90`.

## Проверочный smoke-test

Для обычной проверки раскладки:

```text
lexx:/> keyboard-live -s 30 -i 16 picocalc-keyboard
```

Для поиска scan-code клавиш, которые ещё не описаны, используется полный диагностический режим:

```text
lexx:/> keyboard-live --full -s 30 -i 16 picocalc-keyboard
```

Ожидаемый результат:

- обычные буквы `A..Z` не отображаются как `unknown`;
- основные знаки `` ` - = [ ] \\ ; ' , . / `` не отображаются как `unknown`;
- shifted symbols показывают корректный `character`, а key name соответствует физической базовой клавише;
- `F1..F5` отображаются как `f1`..`f5`;
- `Shift+F1..F5` отображаются как `f6`..`f10`;
- `Shift+Esc` отображается как `break`;
- `Alt+Ins` отображается как `insert`;
- `Shift+Tab` отображается как `home`;
- `Delete` отображается как `delete`, а `Shift+Delete` как `end`;
- после отпускания клавиш `pressed` возвращается в `<none>`.

Если в будущем появится новая неизвестная клавиша, в `--full` нужно смотреть строки
`event: key-down unknown scan=0x.. mods=..` и добавлять mapping по фактическим codes. Если в `--full` нет ни `unknown`,
ни raw/event строк при нажатии конкретной клавиши, значит STM32 firmware не отдаёт это сочетание в keyboard FIFO.

Документ соответствует LexxOS 0.0.7.
