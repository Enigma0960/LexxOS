# Гостевые приложения LexxOS

Программы из этого каталога собираются для RV32I VM, независимо от архитектуры платы и профиля прошивки.

По умолчанию приложения также входят в обычную сборку прошивки (`LEXXOS_APPS=ON`).
Их можно отключить через `-DLEXXOS_APPS=OFF`. Отдельный пресет `apps` задаёт `LEXXOS_APPS_ONLY=ON`
и собирает только приложения независимо от `LEXXOS_APPS`.

Компилятор приложений (`LEXXOS_APP_CXX`) выбирается отдельно от компилятора прошивки (`CMAKE_CXX_COMPILER`).
Обычный системный GCC для архитектуры компьютера не создаёт RV32I-код; используйте RISC-V GCC
или Clang с целевой архитектурой RISC-V.

## Сборка всех приложений

Из корня репозитория:

```sh
cmake --preset apps
cmake --build --preset apps
```

Общий таргет `apps` собирает все зарегистрированные приложения. Готовые файлы находятся в `build/apps/*.lexx`
(включая `fileman.lexx`, `pacman.lexx`, `snake.lexx` и `tetris.lexx`); их можно скопировать на SD-карту. Объекты, ELF,
BIN и MAP остаются в `build/apps-cmake/`. Сборка инкрементальная: учитывает исходники, подключённые заголовки, linker
script, manifest и упаковщик. Изменение manifest требует только повторной упаковки.

Для отдельного приложения:

```sh
cmake --build build/apps-cmake --target app-fileman
```

Нужны CMake 3.25+, Ninja, Python 3 и RISC-V GCC с соответствующим `objcopy`, либо Clang с поддержкой RISC-V,
LLD и `llvm-objcopy`. SDK плат не нужны. По умолчанию сначала ищется RISC-V GCC, затем Clang.
Инструменты можно задать явно при конфигурации:

```sh
cmake --preset apps \
  -DLEXXOS_APP_CXX=/path/to/riscv64-elf-g++ \
  -DLEXXOS_APP_OBJCOPY=/path/to/riscv64-elf-objcopy
```

При смене компилятора рекомендуется удалить `build/apps-cmake` и сконфигурировать сборку заново.
Старый вызов `make -C apps/fileman` перенаправляется на тот же CMake-таргет.

## Добавление программы

Создайте подкаталог с исходниками, manifest и `CMakeLists.txt`, вызовите в нём `lexxos_add_app`,
указав `SOURCES`, `MANIFEST`, `MEMORY`, `STACK`, `CAPABILITIES` (пример — `fileman/CMakeLists.txt`).
Добавьте `add_subdirectory(имя)` в `apps/CMakeLists.txt`. Программа автоматически попадёт в общий таргет `apps`,
получит отдельный таргет `app-имя` и результат `build/apps/имя.lexx`.
