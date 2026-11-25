# LexxOS Build Tool

GUI-конфигуратор сборки LexxOS. Инструмент предназначен для настройки CMake
профилей, платформ, плат, путей к SDK/toolchain и набора компонентов. Запуск и
отладка остаются в CLion или другой IDE.

## Запуск из исходников

```bash
cd /path/to/LexxOS
python3 -m pip install -r tools/build-tool/requirements.txt
tools/lexxos-build-gui
```

## Экспорт в CMakeUserPresets.json

Конфигуратор может записать выбранную платформу, плату, build directory и набор
компонентов в корневой `CMakeUserPresets.json`. Это локальный пользовательский
файл CMake; он уже добавлен в `.gitignore` и не должен попадать в репозиторий.

Порядок работы:

1. Выбрать платформу и плату.
2. Выбрать базовый preset или `manual`.
3. Настроить компоненты и пути к SDK/toolchain.
4. Указать `User preset name`.
5. Нажать `Export CMakeUserPresets.json`.

После этого можно проверить новый профиль из терминала:

```bash
cmake --preset <user-preset-name>
cmake --build --preset <user-preset-name>-build
```

## Pico picotool path

Для Pico поле `picotool fetch path` по умолчанию использует CMake macro:

```text
${sourceDir}/build/tools/picotool
```

Если в поле указан относительный путь, при экспорте в `CMakeUserPresets.json` он
будет сохранён относительно `${sourceDir}`.

## Проверка CLI-слоя

GUI использует существующие helper-скрипты:

```bash
tools/lexxos-build check all
tools/lexxos-build all
```

## Сохранение настроек

Конфигуратор сохраняет последние введённые значения через `QSettings`. Для каждой
платформы хранится свой набор полей: выбранная плата, preset, build directory,
пути к SDK/toolchain/debugger, дополнительные CMake options и набор компонентов.
При переключении платформы текущие значения сохраняются, а при возврате к этой
платформе восстанавливаются.

Файл `CMakeUserPresets.json` остаётся отдельным явным экспортом и не создаётся
автоматически при каждом изменении поля.

## Быстрые действия в GUI

В конфигураторе есть несколько действий, которые не запускают сборку:

- `Export CMakeUserPresets.json` — записывает текущую конфигурацию как локальный
  конфигурации настройки и сборки CMake.
- `Open CMakeUserPresets.json` — открывает локальный файл preset в системном
  приложении.
- `Open build dir` — открывает build directory текущей конфигурации.
- `Reset platform settings` — сбрасывает сохранённые через `QSettings` поля
  только для текущей платформы.

`CMakeUserPresets.json` по-прежнему создаётся только явно через кнопку экспорта.

## Сборка standalone-приложения

После отладки GUI можно собрать как отдельный исполняемый файл и положить его в
`tools/`, чтобы не требовать установленный PySide6 у пользователя:

```bash
cd /path/to/LexxOS
python3 -m pip install -r tools/build-tool/requirements.txt
python3 tools/build-tool/package.py
```

По умолчанию скрипт собирает GUI через PyInstaller и копирует результат в:

```text
tools/lexxos-build-tool
```

Для чистой пересборки:

```bash
python3 tools/build-tool/package.py --clean
```

Для отладочного console-бинарника:

```bash
python3 tools/build-tool/package.py --console
```

Для указания другого выходного файла:

```bash
python3 tools/build-tool/package.py --output tools/lexxos-build-tool-dev
```

На macOS при необходимости можно убрать quarantine-атрибут:

```bash
xattr -d com.apple.quarantine tools/lexxos-build-tool
```

Standalone-бинарник ищет корень репозитория в таком порядке:

1. переменная окружения `LEXXOS_ROOT`;
2. текущий рабочий каталог;
3. каталог, рядом с которым лежит сам исполняемый файл;
4. исходный путь `main.py` при запуске из исходников.

Обычный сценарий: положить бинарник в `tools/` и запускать его из корня LexxOS.
