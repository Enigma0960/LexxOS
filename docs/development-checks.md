# Проверки разработки LexxOS

Документ описывает быстрые проверки, которые выполняются на host-сборке и не требуют MCU, Pico SDK, STM32 toolchain
или реального оборудования.

## Локальный набор проверок

Архитектурная очистка Stage 0 началась в `0.0.5`, а к `0.0.7` правило C++ layout пересмотрено под фактическую структуру
кода: host-сборка, contract tests, Markdown-проверка и C++ layout check должны проходить вместе. В
implementation-файлах не допускаются file-scope `using namespace` директивы. Implementation-local helper-типы в `.cpp`
разрешены, если они остаются внутренней деталью одного translation unit и не становятся контрактом между модулями.

Для проверки переносимого кода выполните полный host-check набор:

```bash
cmake -S . -B build/host-checks \
  -DLEXXOS_PLATFORM=host \
  -DLEXXOS_BOARD=host_native \
  -DBUILD_TESTING=ON \
  -DLEXXOS_ENABLE_STATIC_CHECKS=ON

cmake --build build/host-checks
ctest --test-dir build/host-checks --output-on-failure
```

Ожидаемый результат: все CTest-проверки проходят, включая `lexxos_cpp_layout_check`, `lexxos_markdown_check`,
`lexxos_board_preset_check`, build/run пары C++ contract tests, `lexxos_gameboy_settings_tests` и
`lexxos_vfs_file_string_tests`.

## C++ contract tests

Быстрые C++ contract tests подключены через CTest только для `LEXXOS_PLATFORM=host`:

```bash
cmake -S . -B build/host-tests \
  -DLEXXOS_PLATFORM=host \
  -DLEXXOS_BOARD=host_native \
  -DBUILD_TESTING=ON

cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure
```

CTest сначала собирает нужные test targets через build fixtures, затем запускает сами исполняемые файлы. Основной executable для smoke-теста stdin собирается командой `cmake --build` перед CTest.

Эти тесты предназначены для проверки небольших переносимых контрактов без запуска Python simulator regression suite:
аргументы программ, input/lifecycle contracts, OS/services contracts, VFS file-string helper и parser настроек Game Boy
runtime.

## Статические проверки проекта

Статические проверки подключаются опционально через `LEXXOS_ENABLE_STATIC_CHECKS`, чтобы их можно было запускать
отдельно
от firmware-сборок и аппаратных тестов:

```bash
cmake -S . -B build/host-checks \
  -DLEXXOS_PLATFORM=host \
  -DLEXXOS_BOARD=host_native \
  -DBUILD_TESTING=ON \
  -DLEXXOS_ENABLE_STATIC_CHECKS=ON

ctest --test-dir build/host-checks --output-on-failure
```

Для подготовки изменений к следующему этапу `LEXXOS_ENABLE_STATIC_CHECKS=ON` считается обязательной локальной проверкой.
Она не заменяет сборку firmware-профилей, но фиксирует быстрый архитектурный gate для переносимой части проекта.

Проверки можно запускать напрямую:

```bash
python3 tools/check_cpp_layout.py .
python3 tools/check_markdown.py .
python3 tools/check_board_presets.py .
```

Механическую проверку `.clang-format` перед релизом можно запускать отдельно:

```bash
git ls-files -z '*.cpp' '*.h' '*.hpp' '*.c' '*.cc' '*.cxx' \
  ':!:third_party/**' ':!:build/**' ':!:cmake-build-*' \
  | xargs -0 clang-format --dry-run --Werror
```

Если `clang-format` не находится в `PATH`, замените имя команды на абсолютный путь установленного бинаря.

По умолчанию `check_cpp_layout.py` сканирует только исходные области проекта: `src`, `emulator/native` и `tests/cpp`.
Это исключает локальные виртуальные окружения, build-каталоги и сторонние зависимости из отчёта. При необходимости
можно явно задать область:

```bash
python3 tools/check_cpp_layout.py . --scan-root src/lexxos/shell
```

## Проверка C++ layout

`tools/check_cpp_layout.py` проверяет implementation-файлы:

- не добавлены ли новые `using namespace` директивы.

Правило соответствует принятой структуре проекта: публичные типы, backend-классы, RAII guards, device/app contracts и
межмодульные структуры должны находиться в `.h`/`.hpp`. В `.cpp` допустимы маленькие implementation-local helper-типы:
строки таблиц, callback contexts, parser state, scripted test doubles и platform-specific bridge objects, если они
используются только внутри одного файла и не нужны для независимого тестирования.

Для локальных helper-символов допустимы anonymous namespace, точечные `using some::symbol;` объявления или явная
квалификация имени. File-scope `using namespace ...;` в implementation-файлах не используется, потому что он размывает
границы слоя и скрывает зависимости между модулями.

Для точечной ревизии старого Stage 0 правила можно запустить диагностический режим:

```bash
python3 tools/check_cpp_layout.py . --forbid-cpp-types
```

Этот режим намеренно не является release gate и не формирует самостоятельный todo backlog: он нужен только как подсказка
перед конкретным refactor, когда локальный тип уже стал самостоятельным контрактом и заслуживает собственного
`detail`-заголовка или пары файлов.

## Проверка Markdown

`tools/check_markdown.py` проверяет документацию:

- закрыты ли fenced code blocks;
- существуют ли локальные Markdown-ссылки на файлы и каталоги внутри репозитория.

Внешние ссылки, `mailto:` и ссылки на anchor внутри текущего документа не проверяются.
Загружаемые ESP-IDF зависимости в `managed_components/` исключены, как и `third_party/`:
их документация не является частью проверяемой документации LexxOS.

## Проверка board presets

`tools/check_board_presets.py` проверяет, что CMake presets для PicoCalc используют корректную пару board profile и Pico
SDK board:

- `picocalc_pico_w -> PICO_BOARD=pico_w`;
- `picocalc_pico2w -> PICO_BOARD=pico2_w`.

## Проверка документации команд

Перед релизом справочник генерируется отдельно для каждой собранной конфигурации:

```bash
python3 tools/generate-command-docs.py \
  --build-dir build/full-host_native \
  --host build/full-host_native/src/lexxos
python3 -m unittest discover -s tests/python -p test_command_docs.py
```

Правила выбора metadata, simulator и UART transcript описаны в [документации генератора](command-docs.md).
Результат по умолчанию: `build/full-host_native/command-docs/`.

## Политика включения

Рекомендуемый порядок:

1. Запускать host-check набор перед переходом между этапами разработки и перед крупными изменениями архитектуры.
2. Для небольших правок запускать хотя бы прямые проверки `tools/check_cpp_layout.py` и `tools/check_markdown.py`.
3. CI запускает host checks для minimal/full и отдельную full simulator regression.
   Workflow: [checks.yml](../.github/workflows/checks.yml). Firmware builds ещё предстоит подключить.

## Headless simulator regression

Полный набор включает headless Qt-тесты интерфейса. Из корня репозитория:

```bash
uv sync --project emulator --locked
uv run --project emulator python emulator/lexxos_emu/gui.py --profile full --build-only
QT_QPA_PLATFORM=offscreen PYTHONPATH=emulator uv run --project emulator python -m pytest emulator/tests \
  --lexxos-sim-lib build/full-dev-sim/src/liblexxos_sim.dylib -q
```

На Linux используйте `.so` вместо `.dylib`. Явно указанный отсутствующий файл — ошибка
проверки. Без явного пути fixture ищет full public build; если его нет, интеграционные
тесты пропускаются. Без PySide6 GUI-тесты также пропускаются, поэтому CI устанавливает
все зависимости из `emulator/uv.lock`. Audio tests используют fake backend.
