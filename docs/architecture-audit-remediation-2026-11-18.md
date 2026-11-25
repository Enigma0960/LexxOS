# Исправления архитектурного аудита — 18 ноября 2026

Продолжение [прохода 15 ноября](architecture-audit-remediation-2026-11-15.md).
Проверки выполняются на host по указанию пользователя.

## Закрытые замечания

### 12 — target-local регистрация устройств

`registerBoardDevices` выполняет переданный `std::span` провайдеров без
`LEXXOS_PLATFORM_*` switch. CMake подключает один target-local список для
host, Pico, STM32F4, ESP-IDF, simulator или RK3506. Списки задают порядок
инициализации. Общий runner сохраняет GPIO binding, настройку ожидания
транзакций и резервирование адреса southbridge.

Legacy GPIO/I²C/SD wiring выделен в явно подключаемый провайдер. RK3506
использует собственный product composition и не получает legacy `sd0`.
Некорректный список с null-provider отклоняется до побочных эффектов;
ошибка GPIO binding предотвращает вызов провайдеров.

Host-контракт проверяет порядок и передачу зависимостей, пустой/некорректный
список, ошибочный GPIO, алиасы общей физической I²C-шины и раздельные шины.
Существующие статические владельцы устройств и legacy board descriptor
сохраняются: замечания 1 и 11 этим не закрываются.

### 41 — перенос выбора memory profile в build composition

`runtime_profile.h` содержит переносимые определения и проверку инвариантов.
`selected_runtime_profile.h` предоставляет constexpr-профиль выбранной цели.
CMake генерирует последовательность base → chip → platform → board,
проверяет совместимость слоёв и передаёт include paths/макрос заголовка
только соответствующей цели. SDK и RK3506, включая bring-up без shell,
используют одну функцию генерации. Все прямые потребители обновлены.

Бюджеты не изменены; `minimal`, `standard`, `full` сохраняют лимиты выбранной
платы. Проверены RP2040/RP2350, варианты heap override, Pico без FreeRTOS,
ESP32/S3/P4 и Lyra. Добавлены негативные configure-тесты и компиляция
переносимого заголовка без config/SDK include paths.
[Числовой отчёт](runtime-memory-report-2026-11-18.md) фиксирует результат.

### 51 — отдельные инспекторы команды info

CLI сохраняет путь, проверку workspace, stat и dispatch. `file_info` отвечает
за определение типа, metadata, timestamps, FAT attributes и счётчик каталога;
`lexx_info` — за bounds заголовка/footer, CRC и bounded workspace;
`shortcut_info` — за shortcut details. `ManifestPrinter` вынесен в собственную
пару `.h/.cpp`. Общие числовые/path helpers отделены от storage diagnostics,
чтобы контрактные тесты использовали настоящее форматирование без SD backend.

Shortcut-инспектор использует VFS контекста через новый overload `load(Vfs&, …)`.
Legacy `load(path, …)` остаётся совместимым. Полная миграция shortcut/autostart
и остальных глобальных VFS callers остаётся частью пункта 7.

Тесты проверяют прежний текст metadata, каталоги, отсутствие файла, timestamps,
FAT attributes, разные shortcuts по одному пути в двух VFS, повреждённый
shortcut, валидный/legacy LEXX, overflow image extent, oversized manifest,
CRC mismatch и недостаток workspace. Обнаружен и исправлен старый дефект
JSON-валидатора: `01`, `1+2`, `1e` больше не принимаются как числа.
Печать полей начинается только после валидации всего манифеста.

## Результаты проверки

После всех изменений сборка и полный CTest прошли для каждого host-профиля:

| Профиль                    | Результат |
|----------------------------|----------:|
| minimal                    | 166 / 166 |
| standard, с UI_NATIVE_DEMO | 166 / 166 |
| full                       | 166 / 166 |

Отдельно прошли 12 Python-тестов каталога команд, 5 негативных тестов выбора
memory profile, `check_cpp_layout.py`, `check_runtime_boundaries.py`,
`check_markdown.py`, `git diff --check` и clang-format dry-run новых/изменённых
исходников этого прохода. Полные журналы находятся в
`work/architecture-audit-20261118/{minimal,standard,full}-final-{build,ctest}.log`.

## Границы проверки

Это host-приёмка архитектурных изменений. SDK/toolchain builds для всех MCU
и исполнение аппаратных провайдеров на платах не выполнялись.
Host-компиляция профилей не является замером heap/stack на MCU.
Изменения MIDI/RTC из другого прохода не переоцениваются этим отчётом.

В актуальном аудите остаётся **43** открытых пункта; номера сохранены.
