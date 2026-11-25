# Архитектурный аудит: исправления и host-приёмка 15.11.2026

Продолжение [аудита 12 ноября](LexxOS_architecture_style_audit_2026-11-12.md).
Номера ниже относятся к исходной таблице из 54 пунктов. Этот проход закрывает
перечисленные программные изменения; весь архитектурный аудит ещё не закрыт.
Аппаратная приёмка не выполнялась: для этого прохода выбран host.

## Закрытые программные замечания

| №  | Изменение                                                                                                                                                                                                                                                  | Проверка                                                                                                                                                                               |
|----|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 5  | `ProgramInputService` владеет очередью и таблицей process modes; шина событий передаётся явно. `Runtime` владеет сервисом, `OperatingSystem`/`AppContext` передают его потребителям. Завершение supervised task освобождает mode до публикации `Finished`. | Два OS context с одинаковым process ID, отдельные очереди/шины, независимые курсоры, bounded mode table, освобождение и повторное использование слота.                                 |
| 6  | `ConsoleInputService` владеет ring buffer и режимом. Shell, runtime keyboard routing, VM, native UI, редактор и raw-transfer guards используют сервис текущей OS.                                                                                          | Независимые injected bytes/raw mode, очередь ESC, сохранение другой очереди при чтении/очистке; CLI stdin; RAII raw-mode guard YMODEM (сам протокол передачи не прогонялся).           |
| 9  | SDMMC CLI/registration перенесены в `apps/storage/rk3506/sdmmc_program.*`; backend возвращает `SdmmcStatus`/`SdmmcResult`, не включает app/program API.                                                                                                    | Host-компиляция backend без исполнения MMIO; настоящая команда с подставным hardware boundary: status, probe success/failure, mounted guard, scan, read, invalid/out-of-range LBA.     |
| 14 | `SdHotplugPolicy` отделяет debounce/state transitions от GPIO, event dispatch и планировщика. `Runtime` передаёт `SystemEventService` собственную шину событий.                                                                                            | Fake media/clock, дребезг, вставка/извлечение, отсутствие повторных событий, независимые экземпляры, остановка ожидания при shutdown; прежние интервалы 25 ms и три samples сохранены. |
| 24 | Нетривиальные методы `tree::Resolver` перенесены в `guest_ui_tree_layout.cpp`; `.h` содержит контракт.                                                                                                                                                     | CTest UI tree/system UI: validation, measure/place, focus, v2/v3 и patch cases.                                                                                                        |
| 25 | Нешаблонный parser/validator/directory lookup ресурсов перенесён в `lexx_resources.cpp`; шаблоны visitor и простые constexpr readers остаются в header.                                                                                                    | LXRS/LXR2, поиск и directory listing, bounds, CRC corruption, неверный offset directory и aliases; packer contract tests.                                                              |
| 42 | Load/save Game Boy settings принимают `Vfs&`; launcher и emulator передают `context.vfs()`.                                                                                                                                                                | Две VFS с одинаковым mount path: создание defaults, save/load, отсутствие записи в соседнюю FS, распространение sync failure.                                                          |

## Исправления baseline и регистрации

- Тестовые периферийные заглушки переведены на `LegacyBoardDescriptor`; identity tests сохранены.
- В UI test targets добавлены пропущенные реализации `GuestView` и `DisplayDevice`.
- Проверка уже установленного фокуса различает успешную установку и отсутствие изменения.
  Отдельно исправлен переход горизонтального фокуса в другой ряд при отключённом соседе.
- `UiSession`, native UI demos и самостоятельные SDK widget groups разделены по файлам;
  implementation-файлы больше не содержат найденные `using namespace` директивы.
- Нативные UI demos перенесены из ручной регистрации `Runtime` в общий command catalog.
  Списки исходников, регистрация и metadata теперь совпадают при включённом
  `LEXXOS_FEATURE_UI_NATIVE_DEMO`. `opt_in` сохраняет прежнее независимое включение флагом.
- `check_runtime_boundaries.py` входит в static CTest checks. Это лексический gate
  против возврата прямых global input calls, global VFS calls в settings и app includes
  в SDMMC backend; он не заменяет AST/include-graph аудит.

## Совместимость и незакрытые границы

- Free-function input API и старые OS constructors сохранены как compatibility adapters.
  Независимая composition передаёт собственные VFS, ProgramInputService и ConsoleInputService.
  Зависимости должны пережить OS, её contexts и все работающие задачи.
- `ProgramSupervisor`/focus, scheduler, console mirror/graphics ownership и часть VFS
  consumers остаются глобальными. Проверена изоляция input/event/VFS сервисов при явной
  composition, но не объявлена полная независимость двух одновременно работающих Runtime.
- Сохранён прежний SPSC-контракт console ring buffer. SMP/multi-producer stress,
  ISR lifetime, освобождение DMA, аппаратный hotplug и teardown периферии не приняты host-тестами.
- Settings I/O workspace пока остаётся общим; тесты двух VFS проверяют последовательные
  обращения. Параллельная работа settings требует отдельного ownership/synchronization шага.
- SDMMC GPT/FAT diagnostics вне `sdmmc_probe.cpp` сохраняют существующие compatibility bridges.
  Cold/warm boot и реальный CMD17 на плате в этом проходе не проверялись.
- Остальные строки исходного аудита не считаются закрытыми этим отчётом. Изменения MIDI/RTC,
  выполненные параллельно в рабочем дереве, сохранены; их архитектурная приёмка здесь не заявляется.

## Воспроизводимость

Рабочие каталоги и полные логи: `work/architecture-audit-20261114/`.
В процессе работы исходные изменения были включены внешним коммитом `199e1b83`;
последующие проверки выполняются на актуальном рабочем дереве поверх него.

```bash
cmake --preset full-host_native -B work/architecture-audit-20261114/host-full \
  -DBUILD_TESTING=ON -DLEXXOS_ENABLE_STATIC_CHECKS=ON
cmake --build work/architecture-audit-20261114/host-full -j 6
ctest --test-dir work/architecture-audit-20261114/host-full --output-on-failure -j 6

cmake --preset minimal-host_native -B work/architecture-audit-20261114/host-minimal \
  -DBUILD_TESTING=ON -DLEXXOS_ENABLE_STATIC_CHECKS=ON -DLEXXOS_APPS=OFF
cmake --build work/architecture-audit-20261114/host-minimal -j 6
ctest --test-dir work/architecture-audit-20261114/host-minimal --output-on-failure -j 6

cmake -S . -B work/architecture-audit-20261114/host-standard -G Ninja \
  -DLEXXOS_PLATFORM=host -DLEXXOS_BOARD=host_native -DLEXXOS_BUILD_FLAVOR=standard \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DLEXXOS_ENABLE_STATIC_CHECKS=ON \
  -DLEXXOS_APPS=OFF -DLEXXOS_FEATURE_UI_NATIVE_DEMO=ON
cmake --build work/architecture-audit-20261114/host-standard -j 6
ctest --test-dir work/architecture-audit-20261114/host-standard --output-on-failure -j 6
```

Итоговый CTest-прогон: **164/164** в каждом из `full`, `minimal` и `standard`.
В `standard` дополнительно включены обе native UI demos; проверка реального `help`
совпадает с command metadata. Прошли `git diff --check`, static layout/boundary/Markdown
checks и `clang-format --dry-run --Werror` для новых policy/SDMMC/parser test файлов.
После добавления проверки RAII raw-mode guard повторно собран и успешно запущен
`lexxos_system_contract_tests` для каждого из трёх профилей.
Размеры MCU `.text/.bss/heap/stack` и производительность SD/LCD этим host-прогоном не измерены.
