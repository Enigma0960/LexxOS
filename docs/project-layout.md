# Структура проекта

## Постоянные исходники

| Каталог | Содержание |
|---|---|
| `src/` | Исходники ОС: ядро, runtime, встроенные команды, драйверы и platform code |
| `apps/` | Отдельные RV32I `.lexx` приложения; `apps/sdk/` — их SDK, `apps/examples/` — примеры |
| `tools/` | Утилиты сборки, упаковки, прошивки, анализа; `tools/emulator/` — эмулятор, `tools/debug/` — OpenOCD/GDB |
| `ports/` | Интеграции ESP-IDF, U-Boot и загрузочного образа Lyra |
| `third_party/` | Сторонние исходники/бинарники и `third_party/licenses/` с отдельными notices |
| `tests/` | Общие host/contract tests и fixtures; специализированные тесты остаются рядом с портом или эмулятором |
| `config/` | Конфигурация проекта; `config/cmake/` — CMake modules, toolchains, board/command catalog |
| `docs/` | Архитектура, планы, инструкции и протоколы проверок |

`src/lexxos/apps` содержит встроенные команды ОС, а `apps/` — отдельно загружаемые
`.lexx`: это разные уровни. Корневые CMakeLists/Presets, README, roadmap, LICENSE
и настройки Git/IDE остаются штатными точками входа. Новые архитектурные слои из
плана пока не создаются: это организация дерева, не изменение драйверных API.

## Локальные данные

`build/` и `dist/` остаются на своих местах. Остальное локальное рабочее содержимое
собирается в **`work/`**, который исключён из Git и статических проверок:

```text
work/
  tmp/                         прежний temp: эксперименты, отчёты, SDK snapshots
  emulator/sdcard/              прежний temp-img: содержимое SD эмулятора
  emulator/images/              место для постоянных raw SD-образов эмулятора
  backups/rk3506/               дампы карт и обязательные backups прошивки
  cache/pytest/                кеш pytest
  venvs/project/                прежний корневой Python venv
  venvs/emulator/               Python environment эмулятора
  espidf/managed_components/    загруженные компоненты ESP-IDF
  generated/                   сохранённые прежние generated-файлы из корня
  config-backups/               старые копии локальных CMake presets
```

`work` — не синоним «можно удалить»: backups и содержимое SD могут быть уникальными
данными пользователя. Ничего из них автоматически не очищается. Исторические
логи и вложенные checkout в `work/tmp` сохраняются как снимки; абсолютные пути
в их старых build caches не переписываются. Для повторного использования такого
checkout требуется обычная повторная конфигурация.

ESP-IDF Component Manager требует имя `managed_components` относительно project
root. В корне остаётся только совместимая **символическая ссылка** на данные в
`work/espidf/managed_components`; CMake создаёт её при необходимости. Внешний SDK
не патчится. Существующий настоящий каталог нельзя молча перезаписать: сначала
перенесите его содержимое в `work/espidf/managed_components`. Для ОС без разрешения
на symbolic links нужно разрешить их создание до конфигурации ESP-IDF.

На этом компьютере прежний `tools/emulator/.venv` также указывает на
`work/venvs/emulator`. Для новых checkout используйте launcher ниже или явно
задайте `UV_PROJECT_ENVIRONMENT`, чтобы uv сразу создавал environment в `work`.
Старый корневой `dependencies.lock` сохранён как
`ports/espidf/locks/legacy-root.lock`; рабочие ESP-IDF profiles используют свои
существующие target-specific locks в той же папке.

## Эмулятор и SD

Настроенный ранее SD-носитель был **каталогом** `temp-img`, а не raw image.
Теперь он находится в `work/emulator/sdcard`; все файлы сохранены.
GUI-конфигурация CLion обновлена на этот путь.

Из корня проекта:

```sh
sh tools/run-emulator.sh --profile full --sd-dir "$PWD/work/emulator/sdcard"
```

Launcher использует `work/venvs/emulator`. Если нужен настоящий raw образ,
храните его в `work/emulator/images/` и передавайте `--sd-image` вместо `--sd-dir`.
Автоматическое преобразование существующего каталога в новый raw образ в этой
миграции не выполняется. Запись в persistent raw image по-прежнему включается
отдельно через `--sd-writable`.

```sh
export UV_PROJECT_ENVIRONMENT="$PWD/work/venvs/emulator"
uv sync --project tools/emulator --locked
uv run --project tools/emulator python -m pytest tools/emulator/tests
```

## После переноса

Reload CMake Project в CLion. Если старый build cache хранит прежний путь
toolchain, выполните `cmake --fresh --preset ИМЯ` и повторите сборку. Учтите,
что `--fresh` сбрасывает cache-only overrides: нужные значения сохраните в
локальном CMakeUserPresets перед этим. `build` и `dist` не перемещались.
На этом компьютере пути toolchain в существующих CMakeSystem-файлах Lyra уже
обновлены без сброса пользовательских cache values.

Таргеты `flash-rk3506` и `update-rk3506` сохраняют новые backups в
`work/backups/rk3506`. Старые дампы перенесены туда же. Файлы тестов и fixtures,
необходимые для воспроизводимости, должны быть в Git (`tests` или рядом с модулем),
а не только в игнорируемом `work`.
