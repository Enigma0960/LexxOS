# Тесты эмулятора LexxOS

## Клавиатура виртуального дисплея

Щелчок по изображению `Sim display` переносит фокус ввода на виртуальный LCD.
В этом состоянии `Tab`, стрелки, Enter, Escape, буквы и отпускания клавиш
поступают в `SimInputDevice` (как на аппаратной клавиатуре), а не в UART.
Щелчок по консоли возвращает ввод команд. При потере фокуса удерживаемые
клавиши отпускаются; повторения Qt не сбрасывают состояние зажатой клавиши.

Тест фокуса GUI (требуется PySide6):

```bash
QT_QPA_PLATFORM=offscreen PYTHONPATH=tools/emulator \
  python3 -m pytest -q tools/emulator/tests/test_display_input_widget.py
```

В эмуляторе нет отдельной задачи опроса физической клавиатуры PicoCalc.
Поэтому `SimRuntime::inputPushEvent` сохраняет события в диагностическом
`SimInputDevice` и при системном фокусе на приложении также публикует их в
`app::program_input`. Это необходимо для `ui::poll()` и `lexx::poll_key()`.
События публикуются вне mutex `SimRuntime`, чтобы не создавать блокировки
в обработчиках event bus. При фокусе shell событие остаётся только в
диагностической очереди.

Интеграционный тест (требует собранный `build/apps/ui-demo.lexx` и `lexxos_sim`):

```bash
PYTHONPATH=tools/emulator python3 -m pytest -q \
  tools/emulator/tests/test_sim_ui_input.py \
  --lexxos-sim-lib build/host-full/src/liblexxos_sim.dylib
```

Эти тесты запускают `lexxos_sim` из отдельных Python-процессов. Такой режим намеренно выбран для текущего этапа, потому
что simulator backend пока использует host threads и глобальное состояние ядра LexxOS.

## Запуск

Сначала соберите shared library:

```bash
cmake --preset debug-sim
cmake --build --preset lexxos-sim
```

Затем запустите тесты:

```bash
PYTHONPATH=tools/emulator python3 -m pytest tools/emulator/tests \
  --lexxos-sim-lib build/sim/src/liblexxos_sim.dylib
  +```

На Linux путь обычно такой:

```bash
PYTHONPATH=tools/emulator python3 -m pytest tools/emulator/tests \
  --lexxos-sim-lib build/sim/src/liblexxos_sim.so
  +```

Также можно передать путь через переменную окружения:

```bash
LEXXOS_SIM_LIB=build/sim/src/liblexxos_sim.dylib \
PYTHONPATH=tools/emulator python3 -m pytest tools/emulator/tests
```

## Что проверяется

- загрузка shell и вывод `help`;
- базовая структура root VFS;
- сценарий `/dev/sd0` без карты;
- монтирование чистого Python-generated FAT16 image;
- чтение `/mnt/sd/README.TXT`;
- обнаружение виртуального OLED-устройства на I2C `0x3C`.

FAT-образ создаётся чистым Python-кодом, без `hdiutil`, `diskutil`, `mkfs.fat` и других внешних утилит.
