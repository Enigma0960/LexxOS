# LexxOS emulator tests

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
PYTHONPATH=emulator python3 -m pytest emulator/tests \
  --lexxos-sim-lib build/sim/src/liblexxos_sim.dylib
  +```

На Linux путь обычно такой:

```bash
PYTHONPATH=emulator python3 -m pytest emulator/tests \
  --lexxos-sim-lib build/sim/src/liblexxos_sim.so
  +```

Также можно передать путь через переменную окружения:

```bash
LEXXOS_SIM_LIB=build/sim/src/liblexxos_sim.dylib \
PYTHONPATH=emulator python3 -m pytest emulator/tests
```

## Что проверяется

- загрузка shell и вывод `help`;
- базовая структура root VFS;
- сценарий `/dev/sd0` без карты;
- монтирование чистого Python-generated FAT16 image;
- чтение `/mnt/sd/README.TXT`;
- обнаружение виртуального OLED-устройства на I2C `0x3C`.

FAT-образ создаётся чистым Python-кодом, без `hdiutil`, `diskutil`, `mkfs.fat` и других внешних утилит.
