# Сборка релизных архивов

Единая команда собирает 12 firmware-архивов и один архив приложений:

```sh
cmake --preset release
cmake --build --preset release
```

Эквивалент без конфигурации CMake:

```sh
python3 tools/release.py
```

Матрица: `m5stack_cardputer_adv`, `picocalc_pico`, `picocalc_pico_w`,
`picocalc_pico2`, `picocalc_pico2w`, `jc_esp32p4_m3_dev`; для каждой minimal/full.
В архив apps попадают только программы, собранные из `apps/` (fileman, pacman,
snake, tetris), не примеры hello/cat из `examples/`.

## Требования

Установить обычные зависимости [сборки проекта](build.md): CMake/Ninja, Python,
ESP-IDF и его Python/toolchains, Pico SDK и Arm toolchain, RV32I guest toolchain.
Инициализировать FreeRTOS submodule на записанном commit. Скрипт не устанавливает SDK
и не подменяет локальные инструменты. Публичные presets определяют версии/пути SDK;
CMakeUserPresets.json и старые sdkconfig в сборку архива не попадают.

Обычный режим требует чистый рабочий каталог и индекс, включая новые неигнорируемые
файлы. Сначала зафиксировать кандидат. Staged-only файл, удалённый на диске, также
считается незавершённым изменением. Скрипт не меняет индекс, не создаёт commit/tag,
не прошивает плату и не публикует файлы на GitHub.

## Выходные файлы

`dist/LexxOS-<version>/` содержит ZIP для каждого выбранного профиля, apps ZIP,
общий `SHA256SUMS` и `release.json`. Внутри каждого ZIP:

- UF2/ELF для Pico либо BIN/ELF, sdkconfig и flash.json для ESP32;
- INSTALL.txt: модуль, порядок установки, команда esptool с фактическими offsets;
- manifest.json с commit, исходным хешем, профилем и SHA256 файлов;
- внутренний SHA256SUMS, лицензии и notices проекта/зависимостей;
- конфигурация платы, feature flags и каталог команд для firmware.

PicoCalc использует southbridge profile `jcs`, как публичные presets; это указано
в инструкции каждого архива. Для factory southbridge нужен отдельный согласованный
профиль, переименовывать JCS-образ в factory нельзя. RP2040 minimal/full сохраняют
ограничения тяжёлых функций по памяти. У Pico/Pico 2 без W отключён CYW43,
board id и имя соответствуют физическому модулю.

Сборка из нового снимка исходников находится в `build/release-runs/run-*/`, рядом
с полными логами каждого шага. Даже apps собираются заново. Ошибка останавливает
матрицу; готовый каталог `dist/LexxOS-<version>` появляется только после успешной
упаковки всей выбранной матрицы. Существующий каталог выпуска не перезаписывается.
Для повторной сборки выбрать новый `--output` либо самостоятельно убрать старый набор.

SDK notices копируются с вложенными лицензиями, чтобы сохранить условия зависимостей.
Успешная компиляция/упаковка не является hardware PASS и не заменяет проверку состава
лицензий при изменении зависимостей. Firmware не включает обновление C6 coprocessor.

## Дополнительные команды

```sh
python3 tools/release.py --plan
python3 tools/release.py --boards jc_esp32p4_m3_dev --flavors minimal full
python3 tools/release.py --apps-only
python3 tools/release.py --jobs 4 --output /path/to/releases
```

Apps добавляются и к ограниченной firmware-матрице. Для проверки незакоммиченной
реализации использовать `--allow-dirty` (в CMake: `-DLEXXOS_RELEASE_ALLOW_DIRTY=ON`).
Получаются явно помеченные `-dev-<source-hash>` архивы; они не представляются
чистой сборкой commit. Копируются фактические рабочие файлы, не staged-копии.
Dirty submodule не допускается и в этом режиме.

Для постоянного выходного каталога CMake поддерживает
`-DLEXXOS_RELEASE_OUTPUT=/path/to/releases`. Target `release-archives` также доступен
из обычных firmware/apps configure, но preset `release` не требует предварительной
конфигурации конкретной платы.

ZIP timestamps фиксированы временем commit, SOURCE_DATE_EPOCH передаётся сборкам.
Это обеспечивает стабильную упаковку одинаковых файлов; побитовая воспроизводимость
всех SDK/compiler результатов на разных машинах отдельно не заявляется.

После проверки набора можно загрузить ZIP, SHA256SUMS и release.json в GitHub Release
обычным способом. Скрипт не требует GitHub token и не выполняет сетевую публикацию.

При компиляции пути исходного снимка и домашнего каталога сборщика заменяются
на `/src/lexxos` и `/opt/build-home` через `-ffile-prefix-map`, включая debug info
и `__FILE__`. Перед созданием ZIP упаковщик проверяет все файлы и метаданные
на фактические локальные пути; при обнаружении сборка набора останавливается.
Для отладки ELF настройте соответствие `/src/lexxos` своей копии исходников.
