# LexxCE: встраиваемое ядро и контроллер освещения на ESP32

**Статус:** реализован первый срез CE-00/CE-01 и отдельный CE blink для ESP-WROOM-32; аппаратная приёмка открыта.

**Дата исходного анализа:** 4 ноября 2026 г.

**Целевая первая платформа:** классический ESP32 под ESP-IDF; далее ESP32-S3 и другие поддерживаемые архитектуры.\
**Предполагаемая лицензия:** наследование применимых условий Apache-2.0 из репозитория и уведомлений сторонних
компонентов; перед внешним распространением проверить состав и лицензии поставки.

> **Область исследования.** Анализ выполнен по исходникам и CMake на дату проверки. Сборка и
> прошивка контроллеров, измерения flash/RAM, анализ схем конкретных плат и аппаратные тесты не проводились. Приведённые
> ниже новые классы, файлы, параметры и REST API являются **предложениями**, кроме явно перечисленного в статусе
> реализации.

Отдельно подготовлен [проект API периферии и хранения параметров](ce-peripherals-storage.md). Правила реакции на жесты
показаны в [примере button_led](../ce/examples/button_led/README.md).

## Подготовительный шаг: документация

5 ноября 2026 года начато выполнение плана с упорядочивания документации:
создана единая [навигация](README.md), материалы RK3506 объединены в
[руководство PicoCalc / Lyra M](boards/picocalc_lyra_m.md), англоязычные
описания переведены. [Статус подготовки](../roadmap.md#подготовка-lexxce)
ведётся в общей дорожной карте. Этот шаг не меняет ядро и не подтверждает
внедрение предложенных ниже классов, API, флагов или исправлений.
Исходный аудит относится к указанной выше ревизии; перед реализацией его
выводы необходимо сверить с актуальным кодом.

## Реализация CE-00/CE-01: первый срез, 05.11.2026

Добавлено общее ядро в `src/lexxos/core/`:

- `CoreSystem` владеет `DeviceManager` и актуальным `CapabilitySet`, хранит ссылки на платформу и шину событий.
- `CoreContext` предоставляет приложению устройства, платформенные сервисы, события и capabilities. Он не зависит от
  `OperatingSystem`, `Shell`, `ProgramManager` или VFS; контекст не копирует состояние ядра.
- `core::initializePlatform()` привязывает backend планировщика, вызывает необязательный platform initialize и
  сбрасывает запрос завершения. Этот общий шаг используется и обычным `Runtime`.
- `core::bootstrap()` дополнительно очищает реестр, вызывает переданный регистратор устройств и обновляет capabilities.
  Он не создаёт shell, VFS, консоль или рабочие задачи.
- `OperatingSystem` теперь содержит `CoreSystem` и делегирует ему прежние методы `devices()`, `platform()` и
  `capabilities()`. Доступ к общему ядру — через `core()`; второго реестра нет.

Минимальная последовательность для встраиваемого приложения:

```cpp
#include <lexxos/core/bootstrap.h>
#include <lexxos/core/core_system.h>
#include <lexxos/platform/platform_services.h>

void registerDevices(lexxos::devices::DeviceManager& devices,
                     lexxos::platform::PlatformServices const& platform);

// Объекты живут весь срок работы приложения.
lexxos::lifecycle::EventBus events;
lexxos::core::CoreSystem system{lexxos::platform::services(), events};

void startApplication() {
    lexxos::core::bootstrap(system, registerDevices);
    auto context = system.createContext();
    // Приложение использует context и создаёт собственные задачи.
}
```

Контракты первого среза:

- Инициализация и регистрация устройств выполняются до запуска рабочих задач. `initialize()` очищает реестр и
  обновляет capabilities, но не переинициализирует оборудование и не очищает события. Повторный bootstrap во время
  работы задач не поддерживается.
- Платформа и EventBus должны жить дольше `CoreSystem`, а ядро — дольше контекстов. По умолчанию используется
  `defaultEventBus()`, что сохраняет общую шину Standard; в тестах или встраиваемом приложении можно передать свою.
- Привязка scheduler backend глобальна для процесса: несколько объектов ядра не создают несколько планировщиков.
- `CapabilitySet` сохраняет действующую семантику Standard: часть возможностей определяется профилем платы, часть —
  зарегистрированными устройствами. Наличие capability не заменяет проверку результата GPIO-операции.

Исправления ESP-IDF:

- Размер стека и high-water mark передаются/учитываются в байтах. Нулевой стек отвергается, малый запрос повышается до
  `configMINIMAL_STACK_SIZE`. Уточнение исходного аудита: в установленном ESP-IDF 6.0.2 для Xtensa `StackType_t` —
  `uint8_t`, поэтому прежние пересчёты на этих платах были численно нейтральны. Теперь код следует контракту API
  напрямую.
- Статический mutex защищает слоты, создание, snapshots и удаление задач между ядрами. Новая задача дожидается
  публикации handle; завершившаяся задача приостанавливается и удаляется только после подтверждения `eSuspended`.
  Callback `visitTasks()` вызывается вне mutex. Приоритет не сужается до восьми бит.
- `reapFinishedTasks()` освобождает завершённые задачи; создание новой задачи также запускает уборку. Standard уже
  вызывает уборку периодически. В CE эту обязанность для возвращающихся задач принимает приложение.
- Для `EventBus` под `LEXXOS_KERNEL_ESPIDF` добавлен отдельный `portMUX` на экземпляр и соответствующие task/ISR
  critical sections. Сохраняется политика вытеснения старых событий; отставший cursor продолжает с самого старого
  доступного события. ISR должен иметь допустимый для FreeRTOS API приоритет. Cache-off/IRAM-вызовы не поддерживаются.
- Для ESP-IDF общий include-каталог профилей памяти подключается как `SYSTEM`, после обычных include-каталогов SDK.
  Локальные `config/FreeRTOSConfig.h` и `config/lwipopts.h` больше не затеняют настройки ядра и сети ESP-IDF.

Аудит GPIO оставляет открытые задачи CE-03: проверку допустимых входных/выходных GPIO средствами SoC, установку
начального уровня до включения output, согласованное изменение пары handler/context и синхронизацию установки ISR
service. Текущий `gpio_install_isr_service(0)` не обещает cache-off исполнение. Аппаратную безопасность реле этот срез
не подтверждает.

Проверки и границы:

- `lexxos_core_contract_tests` собирает только общее ядро и его непосредственные зависимости, регистрирует mock GPIO,
  проверяет bootstrap, внедрение сервисов, события и актуальность capabilities. В зависимостях публичных заголовков
  нет shell/VFS/program/OS; Standard stubs не используются.
- `lexxos_espidf_scheduler_1core_tests` и `lexxos_espidf_scheduler_2core_tests` проверяют байты, приоритеты, ошибку
  создания,
  ранний старт/завершение, уборку, одновременное выделение слотов и повторный вход из visitor. Mock использует
  четырёхбайтовый `StackType_t`, чтобы обнаруживать ошибочное преобразование единиц.
- `lexxos_espidf_event_bus_tests` проверяет конкурентные task/ISR-публикации, чтение составного payload и переполнение.
  Это host-модель API, а не испытание реальных прерываний ESP32.
- Контрактные тесты Standard проверяют общий реестр через `OperatingSystem` и `CoreContext`.
- Отдельный component `lexxos_ce`, `LEXXOS_EDITION=ce`, `minimal_blink`, tiny-профиль и аппаратный smoke остаются
  следующими
  шагами. Этот срез не заявляет готовность полной CE-поставки или измеренные бюджеты flash/RAM.

### Проверка первого среза

На текущем рабочем дереве выполнены:

- Host Standard: сборка `lexxos` и smoke stdin; новые CE-тесты и контракты Standard проходят.
- ESP-IDF 6.0.2: успешно собрана цель `app` для `minimal-m5stack_basic_v1` (ESP32) и
  `minimal-m5stack_cardputer` (ESP32-S3). Это проверка совместимости **Standard**, ещё не отдельная CE-прошивка.
- Для восстановления сборки исправлены существовавшие ошибки: несовпадение типов аргументов `std::min` в `guest_ui.h`,
  неполная явная инициализация `MountLookup` в `sd_bench_program.cpp`, повторное имя переменной в host-тесте
  синхронизации и устаревшее ожидание выключенного debug-вывода в тесте standalone FreeRTOS.
- После повторной проверки исправленных тестов проходят 137 из 139 CTest-проверок. Открыты два общих hygiene gate:
  `lexxos_cpp_layout_check` указывает на существующие `using namespace` вне CE;
  `lexxos_markdown_check` — на ссылку на отсутствующий `apps/sdk/lexx/ui/README.md` в изменяемой документации LexxUI.
  Файлы параллельной работы над графическим SDK не исправлялись в рамках CE.
- Прошивка устройств, испытание ISR на реальном оборудовании и измерения потребления памяти не выполнялись.

Воспроизведение основных проверок:

```bash
cmake -S . -B build/ce-host -DLEXXOS_PLATFORM=host -DLEXXOS_BOARD=host_native \
  -DBUILD_TESTING=ON -DLEXXOS_ENABLE_STATIC_CHECKS=ON
cmake --build build/ce-host --target lexxos lexxos_random_tests
ctest --test-dir build/ce-host --output-on-failure

cmake --preset minimal-m5stack_basic_v1 -B build/ce-standard-esp32
cmake --build build/ce-standard-esp32 --target app
cmake --preset minimal-m5stack_cardputer -B build/ce-standard-esp32s3
cmake --build build/ce-standard-esp32s3 --target app
```

## CE-02: ESP-WROOM-32 / blink, 05.11.2026

[SDK для ESP32](../ce/README.md) собирается из корня пресетом `ce-sdk-esp32`.
Поставка включает статическую библиотеку ядра и публичные заголовки.
Список общих исходников Standard и CE хранится в `config/cmake/lexxos_core_sources.cmake`.
При экспорте SDK проверяются состав библиотеки и определения платформы.

[Пример blink](../ce/examples/blink/README.md) — самостоятельный проект ESP-IDF.
Описание ESP-WROOM-32, настройки GPIO, UART, flash и таблица разделов находятся в примере.
Копия папки собирается с установленным ESP-IDF, архивом ядра и каталогом заголовков.
Приложение передаёт ядру описание платы при создании `CoreSystem`.

На тестовой плате используется GPIO2 с активным высоким уровнем. Задача начинает
с выключенного светодиода и переключает его раз в 1000 мс. UART0 на 115200
сообщает о загрузке ядра и успешных переключениях. Host-тесты проверяют обе
полярности, интервал, сообщения и обработку ошибки записи GPIO.

Отдельное уменьшение ёмкостей реестров для tiny-профиля и аппаратный smoke остаются открытыми.
Разделы CE-00/CE-01 выше фиксируют предыдущий срез; их результаты не заменяют эти новые проверки.

## 1. Решение в кратком виде

Создать **LexxCE (Core Embedded)** — отдельный способ компоновки *того же* ядра LexxOS, без shell, `.lexx`-раннера,
пользовательского менеджера программ, дисплея и VFS по умолчанию. Пользовательское приложение компилируется и статически
линкуется с минимально выбранными сервисами LexxCE в одну прошивку. Это не DLL, не динамический модуль, не «система
внутри системы» и не исполнение через эмулятор.

Первая целевая поставка — **ESP-IDF component**, который ESP-IDF компилирует в статическую библиотеку (`.a`) для
конкретного чипа и конфигурации. Дополнительно должен существовать обычный CMake target `lexxos_ce_core` для host-тестов
и будущих платформ. Не пытаться поставлять один универсальный бинарный `.a` для всех ESP32: ABI, toolchain, `sdkconfig`,
IDF, CPU architecture и настройки оптимизации могут отличаться. Переносимость обеспечивается **исходным кодом и
публичным API**, а не идентичным бинарником.

**Ключевой принцип:** `LexxOS Core` владеет платформенными контрактами, устройствами, планированием и событиями;
`LexxOS Standard` добавляет shell/VFS/программы; `LexxCE` предлагает API для встроенного приложения. Ни один из этих
вариантов не должен дублировать реализацию драйвера или логики ядра.

### 1.1. Конечный сценарий

Прошивка умного дома для одной аппаратной платы с 2–4 дискретными входами и 2–4 выходами (например, реле), с локальной
логикой без Wi-Fi, конфигурируемыми сценариями, удалённым REST-управлением, постоянной конфигурацией и безопасной
инициализацией выходов.

Примеры требований:

- короткое нажатие входа №1 переключает связанный канал освещения;
- долгое удержание выключает все *другие* каналы, оставляя текущий без изменений;
- следующее долгое удержание включает другие каналы (либо восстанавливает предыдущие состояния — режим выбирается
  правилом);
- отдельные каналы могут работать в импульсном, переключающем, прямом, таймерном и взаимоблокирующем режимах;
- устройства должны работать и принимать локальные нажатия при отсутствии Wi-Fi, HTTP-сервера, RTC или синхронизации
  времени.

### 1.2. Что сознательно не входит в MVP

Выполнение `.lexx`-файлов, скриптовый интерпретатор, shell/terminal, LCD/TUI, Game Boy, SD/FAT, полноценная
универсальная rule-VM, облачный сервис и редактор сценариев на устройстве. Это могут быть отдельные опциональные
компоненты или внешние редакторы конфигурации.

---

## 2. Аудит существующей архитектуры

### 2.1. Уже пригодные компоненты

| Существующий компонент                              | Текущее назначение                                              | Применение в CE                          | Решение                                                                           |
|-----------------------------------------------------|-----------------------------------------------------------------|------------------------------------------|-----------------------------------------------------------------------------------|
| `src/lexxos/os/operating_system.*`                  | Композиция `DeviceManager`, `ProgramManager`, VFS, capabilities | Содержит общие концепции                 | **Выделить лёгкий CoreSystem** и сделать `OperatingSystem` расширением, не копией |
| `src/lexxos/platform/platform_services.*`           | Контракты lifecycle, clock, GPIO, console, power                | Хороший адаптер платформы                | **Использовать**, расширяя осторожно и без привязки GPIO к shell                  |
| `src/lexxos/devices/device_manager.*`               | Реестр I2C/SPI/UART/GPIO, блокировки и owner                    | Основной механизм владения ресурсами     | **Повторно использовать**, проверить конкурентность и уменьшить профиль           |
| `src/lexxos/kernel/scheduler.h`                     | Абстракция задач, sleep/yield, мониторинг                       | Основной API задач CE                    | **Использовать**, предварительно исправить ESP-IDF backend                        |
| `src/lexxos/kernel/espidf/scheduler_espidf.cpp`     | Обёртка над IDF FreeRTOS                                        | Нужен для CE на ESP32                    | **Исправить стек/лимиты и жизненный цикл**                                        |
| `src/lexxos/lifecycle/event_bus.*`                  | Шина событий фиксированной ёмкости, ISR/Task publication        | Подходит как концепция                   | **Разобраться с критическими секциями и потерями событий**                        |
| `src/lexxos/runtime/capabilities.*`                 | Описание доступных возможностей                                 | Полезно для app bootstrap                | **Использовать**; добавить профиль network/config/output                          |
| `src/lexxos/runtime/runtime_profile.h`              | Статические лимиты памяти/устройств/очередей                    | Основа CE-профиля                        | **Выделить компактный profile**, не заставлять CE валидировать shell              |
| `src/lexxos/composition/board_device_providers.*`   | Регистрация ресурсов по плате                                   | Принцип пригоден                         | **Добавить профиль платы контроллера**, без M5Stack-зависимостей                  |
| `src/lexxos/platform/espidf/detail/espidf_gpio.cpp` | Настройка и чтение GPIO/IRQ                                     | Может быть низкоуровневым драйвером      | **Повторно использовать после аудита pin validation/ISR**                         |
| `src/lexxos/app/runtime.*` и `runtime_tasks.*`      | Полная инициализация ОС и shell-задач                           | Не является базой минимального bootstrap | **Оставить только для Standard**, общую инициализацию вынести отдельно            |
| `ports/espidf/components/lexxos/*`                  | ESP-IDF component полной ОС                                     | База системы сборки                      | **Разделить component core и стандартную оболочку**                               |

### 2.2. Проблема №1: `Runtime` создаёт shell независимо от flavor

`src/lexxos/app/runtime.h` непосредственно содержит поле `lexxos::shell::Shell _shell`, включает `shell.h`, а
`runtime.cpp` выполняет `bindOperatingSystem`, `registerShellProgram`, `registerBuiltinPrograms` и запускает
`startRuntimeTasks`. Это не headless-компонент в нужном смысле. Нельзя получить настоящий минимальный CE только
параметрами `LEXXOS_FEATURE_DISPLAY=OFF` и `LEXXOS_BUILD_FLAVOR=minimal`.

**Решение:** вынести общие этапы lifecycle / device composition из `Runtime::initializeSystem()` в
`lexxos::core::bootstrap(...)` (или `CoreSystem::initialize(...)`). `StandardRuntime` строится поверх него и
дополнительно включает shell, manager, VFS и привычные runtime tasks. `CeRuntime` запускает только embedded
application + выбранные сервисы. При этом **старые точки входа и поведение Standard сохраняются**.

### 2.3. Проблема №2: источник компонентов монолитен

В `config/cmake/lexxos_sources.cmake` группа `LEXXOS_RUNTIME_SOURCE_FILES` содержит app runtime, console, LCD-консоль,
`AppContext` и `OperatingSystem`; `LEXXOS_COMMON_SOURCES` собирает shell, VFS и программные компоненты вместе. И в
ESP-IDF `idf_component_register(SRCS espidf_main.cpp ${LEXXOS_COMMON_SOURCES} ...)` добавляет их в один компонент.
Удаление неиспользуемых секций линкером **не заменяет** корректное разделение зависимостей (нежелательные translation
units, static objects, init hooks и ссылки могут оставаться).

**Решение:** создать исходные группы по независимым возможностям:

```text
LEXXOS_CORE_SOURCES       kernel contract, capabilities, clocks, basic lifecycle
LEXXOS_DEVICE_CORE_SOURCES device manager, resources, GPIO, bus foundations
LEXXOS_CE_SOURCES         embedded bootstrap, service host, app lifecycle
LEXXOS_VFS_SOURCES        optional filesystem
LEXXOS_PROGRAM_SOURCES    optional program manager / guest launcher
LEXXOS_SHELL_SOURCES      optional terminal/shell
LEXXOS_STANDARD_SOURCES   traditional runtime, console tasks
LEXXOS_OPTIONAL_...       display, SD, audio, gameboy, etc.
```

Более тонкое разбиение делается по реальным include/link зависимостям. У каждой группы должен быть ровно один владелец и
однозначные dependencies.

### 2.4. Проблема №3: `OperatingSystem` и `AppContext` пока тяжелы для CE

`OperatingSystem` содержит `ProgramManager`, `DeviceManager`, VFS; `AppContext` включает `program.h`, `vfs.h`, а в
реализации обращается к shell и program supervisor. Если CE использует `OperatingSystem` как есть, то на уровне API
сохраняются зависимости, которые цель CE предполагает исключить.

**Рекомендуемый контракт:**

```text
CoreSystem  ---> DeviceManager, PlatformServices, Clock, EventBus, Capabilities
     ^
     |
Standard OperatingSystem ---> ProgramManager, VFS, AppContext, Shell
     |
StandardRuntime

CeHost ---> CoreSystem + optional CE services ---> EmbeddedApplication
```

`AppContext` оставить как есть для совместимости `.lexx` и shell-программ; новый `CoreContext` не должен включать
`Shell`, `ProgramManager` или VFS. При необходимости из `AppContext` делегировать общие методы в `CoreContext`, а не
поддерживать две независимые копии устройства и capability state.

### 2.5. Проблема №4: реальные ошибки/риски ESP-IDF в текущем коде

**P0-A. Единицы размера стека.** `src/lexxos/kernel/espidf/scheduler_espidf.cpp` делает
`stackSizeBytes / sizeof(StackType_t)` перед передачей `stackDepth` в `xTaskCreate`/`xTaskCreatePinnedToCore`. Но для
ESP-IDF аргумент stack size этих API задаётся **в байтах**. Также `uxTaskGetStackHighWaterMark()` в IDF возвращает
**байты**, а код дополнительно умножает значение на `sizeof(StackType_t)`. В текущем виде потенциальны недовыделение
стека и неверные диагностические данные. Исправить и покрыть тестом перед CE. На других FreeRTOS backend не применять
ESP-IDF семантику автоматически.

**P0-B. Шина событий и ISR.** `lifecycle/event_bus.cpp` включает FreeRTOS locking только под `LEXXOS_KERNEL_FREERTOS`, а
ESP-IDF component устанавливает `LEXXOS_KERNEL_ESPIDF=1`. Если второй макрос не определён вне просмотренных путей, на
ESP-IDF вызовы `publish`/`publishFromIsr` проходят без защиты. Требуется проверить фактический набор compile definitions
в build output и реализовать платформенно корректный ISR-safe backend. Для dual-core ESP32 не достаточно предполагать,
что локального запрета прерываний всегда хватит: нужен подходящий кросс-ядерный `portMUX_TYPE`/critical section или
многопроизводительная очередь, с отдельно определёнными Task и ISR API.

**P0-C. Слоты задач и конкурентность.** ESP-IDF backend содержит фиксированный `constexpr maxTasks = 8` и статический
массив `g_tasks`; `allocateTaskSlot()` и `taskTrampoline()` меняют его без видимой взаимной синхронизации. Для CE с
Wi-Fi/REST/логикой это реальный риск при одновременном старте и завершении задач. Требуются синхронизация, явное
удаление/статус, владелец задач и настраиваемый бюджет. Лучше не учитывать собственные задачи ESP-IDF (Wi-Fi/lwIP/httpd)
как задачи LexxOS, но учитывать их в общем RAM budget.

**P0-D. GPIO ISR.** `espidf_gpio.cpp` устанавливает глобальный обработчик на GPIO, берёт handler из массива и вызывает
callback непосредственно из ISR. Callback приложения **не должен** исполнять сложную логику, писать в NVS, вызывать
HTTP, выделять память или блокироваться. Необходимо проверить семантику установки/повторной установки ISR service,
допустимые пины, принадлежность ресурса и соответствие режима IRAM-safe (особенно во время операций с flash).

**P1. Глобальные singleton-и.** В коде есть `platform::services()`, `defaultEventBus()`, глобальный scheduler backend и
статические board objects. Для первого embedded-инстанса на микроконтроллере допустимо, но контракт должен явно
ограничивать количество экземпляров, повторную инициализацию и порядок завершения. Host-тестам желательно иметь инъекции
интерфейсов вместо скрытых глобальных зависимостей.

**P1. GPIO ownership.** `DeviceManager` уже умеет `registerGpioPin` и `lockGpioPin`, но низкоуровневые
`PlatformServices.gpio` остаются свободно вызываемыми функциями. CE должен предоставить приложение-ориентированный
`GpioLease`/`InputChannel`/`OutputChannel`, через которые контролируются права и конфликтующие владельцы. Низкоуровневые
функции остаются внутри board driver; полностью защитить С++ от обхода HAL на bare-metal невозможно, но публичный
контракт должен его исключать.

**P1. Избыточные статические лимиты.** `runtime_profile.h` включает обязательные shell/program/VFS limits;
`DeviceManager` держит массивы неиспользуемых типов устройств. Требуется отдельный CE profile и проверки, не требующие
несуществующих функций.

### 2.6. Что **не** следует исправлять через CE

Не делать переписывание всех драйверов «с нуля», второй планировщик, собственный Wi-Fi стек, собственный HTTP parser или
параллельный форк ESP-IDF. Для ESP32 целесообразно использовать **ESP-IDF как BSP/RTOS/Network stack** и LexxCE как
слой ядра, владения ресурсами, сервисов и встраиваемого приложения.

---

## 3. Архитектура продукта

```text
                  ПРОШИВКА SMART HOME
       +-------------------------------------+
       | home_lights/application             |
       | - LightController                   |
       | - ButtonGestureRecognizer          |
       | - RuleEngine / GroupScenes          |
       | - RestRoutes / Config               |
       +-------------------+-----------------+
                           |
       +-------------------v-----------------+
       |            LexxCE               |
       | Embedded Host / CoreContext         |
       | Device registry / resource leases   |
       | Event loop / timers / task adapters  |
       | Service host / capabilities / logs   |
       +-----------+-------------+-----------+
                   |             |
         +---------v-----+  +----v-----------+
         | Core shared   |  | Optional CE    |
         | with Standard |  | modules        |
         | HAL, devices  |  | Wi-Fi/NVS/HTTP |
         +---------+-----+  +----+-----------+
                   |             |
       +-----------v-------------v-----------+
       | ESP-IDF / FreeRTOS / lwIP / Drivers |
       +-------------------+-----------------+
                           |
       +-------------------v-----------------+
       | ESP32 + GPIO inputs + relay outputs |
       +-------------------------------------+

  Параллельно: StandardRuntime -> тот же Core shared
                    + Shell/VFS/Programs/Display
```

### 3.1. Границы владения и ответственности

| Слой                    | Отвечает за                                                                                       | Не отвечает за                                               |
|-------------------------|---------------------------------------------------------------------------------------------------|--------------------------------------------------------------|
| ESP-IDF                 | Boot, RTOS scheduling, GPIO hardware, Wi-Fi stack, NVS driver, HTTP transport, OTA backend        | Бизнес-правила освещения                                     |
| Shared LexxOS Core      | Устройства, их владельцы, события, сервисы времени и задач, lifecycle, диагностическая информация | Политики управления конкретной квартирой                     |
| LexxCE                  | Инициализация без shell, embedded app hooks, зависимости и контракт компонентов                   | Конкретные GPIO-пины, названия сцен и пользовательские rules |
| Board profile           | Пины, активные уровни, электробезопасные значения, ограничения ввода/вывода                       | Семантика long press                                         |
| Home Lights Application | Свет, кнопки, scenes, REST schema, persistence policy                                             | Прямые вызовы небезопасных аппаратных ISR                    |

### 3.2. Структура репозитория (проектируемая)

```text
LexxOS/
  src/lexxos/
    core/                   # НОВОЕ: CoreSystem, bootstrap, shared context
    ce/                     # НОВОЕ: CeHost, app lifecycle, service registry
    kernel/                 # СУЩЕСТВУЕТ: scheduler, backend, synchronization
    devices/                # СУЩЕСТВУЕТ: device manager / resource ownership
    lifecycle/              # СУЩЕСТВУЕТ: event bus, расширить/исправить
    platform/               # СУЩЕСТВУЕТ: platform services, hardware backends
    os/                     # СУЩЕСТВУЕТ: расширение для полной ОС
    shell/                  # СУЩЕСТВУЕТ: только Standard
    program/                # СУЩЕСТВУЕТ: только Standard/опционально
  include/lexxos/ce/        # НОВОЕ: публичные стабильные заголовки CE
    host.h
    application.h
    context.h
    devices.h
    status.h
  config/cmake/
    lexxos_sources.cmake    # РЕФАКТОРИНГ: общие source groups
    LexxOSCore.cmake        # НОВОЕ: Core/CE build targets
  config/memory/boards/
    home_esp32_relay4.h     # НОВОЕ: лимиты памяти, если требуется
  ports/espidf/components/
    lexxos_core/            # НОВОЕ: базовый IDF component
    lexxos_ce/              # НОВОЕ: embedded runtime (depends core)
    lexxos_ce_http/         # НОВОЕ: опциональный HTTP adapter
    lexxos/                 # СУЩЕСТВУЕТ: Standard; миграционный wrapper
  examples/ce/
    minimal_blink/
    esp32_relay4/
  tests/ce/
    core_contract/
    app_lifecycle/
    gpio_events/
    rule_engine_host/
```

**Вариант с меньшим числом компонентов:** на первом патче можно объединить `lexxos_core` и `lexxos_ce` в один **IDF
component `lexxos_ce`**, если при этом уже существует независимый CMake target с чистыми source groups. Превращение в
множество пакетов до проверки реальных link-зависимостей может увеличить сложность без пользы.

### 3.3. Модель включения возможностей

Предлагаются **ортогональные** параметры, не смешивающие выбор режима запуска с флагами устройства:

```cmake
LEXXOS_EDITION=standard|ce                 # НОВОЕ
LEXXOS_CE_PROFILE=tiny|iot|custom         # НОВОЕ
LEXXOS_CE_VFS=OFF
LEXXOS_CE_PROGRAM_MANAGER=OFF
LEXXOS_CE_SHELL=OFF
LEXXOS_CE_EVENT_BUS=ON
LEXXOS_CE_GPIO=ON
LEXXOS_CE_WIFI=ON
LEXXOS_CE_HTTP=ON
LEXXOS_CE_PERSISTENCE=ON
LEXXOS_CE_OTA=OFF                           # Включить после MVP
LEXXOS_CE_DIAGNOSTICS=ON
```

Это **проектируемые** параметры; сейчас таких опций в ревизии нет. Для CE запрещать несовместимые конфигурации на этапе
CMake, а не давать ошибку линковки позже. Существующие `LEXXOS_FEATURE_*` использовать для общих аппаратных
возможностей, сохраняя обратную совместимость Standard.

Пример трёх профилей:

| Подсистема                         |    `tiny` |       `iot` | `custom`  |
|------------------------------------|----------:|------------:|-----------|
| Core, scheduler abstraction, GPIO  |        Да |          Да | Явно      |
| Device ownership, события, таймеры |        Да |          Да | Явно      |
| Wi-Fi, REST                        |       Нет |          Да | Явно      |
| NVS/configuration                  | По выбору |          Да | Явно      |
| VFS, shell, `.lexx`                |       Нет |         Нет | По выбору |
| Screen/audio/SD                    |       Нет |         Нет | По выбору |
| OTA                                |       Нет | Опционально | Явно      |

Особенно важно: `tiny` и `iot` **не должны означать** урезанную надёжность, синхронизацию или безопасную политику output
startup.

---

## 4. Публичный C++ API LexxCE

### 4.1. Концепция

Приложение описывает callbacks жизненного цикла и получает один context. В нём есть разрешённые сервисы ядра и
устройства. Запуск принадлежит **приложению-владельцу прошивки**, а не shell. Для ESP-IDF планировщик уже запущен до
`app_main()`: CE не должен внутри обычного `initialize()` безусловно удалять `app_main` task.

```cpp
// ПРЕДЛОЖЕНИЕ, НЕ СУЩЕСТВУЮЩИЙ API
namespace lexxos::ce {

struct BootOptions {
    bool enableDiagnostics{true};
    bool enableEventDispatcher{true};
};

class Context {
public:
    devices::DeviceManager& devices();
    platform::PlatformServices const& platform() const;
    lifecycle::EventBus& events();
    // Через service lookup: network, settings, clock, timers, logger
};

class Application {
public:
    virtual ~Application() = default;
    virtual Status onInitialize(Context& context) = 0;
    virtual Status onStart(Context& context) = 0;
    virtual void onStop(Context& context) = 0;
};

class Host {
public:
    Status initialize(BootOptions const& options);
    Status registerApplication(Application& app);
    Status start();
    void stop();
    Context& context();
};

} // namespace lexxos::ce
```

Этот пример фиксирует намерение, но **не ABI**. До реализации выбрать одну из двух политик: `Application` как
виртуальный C++ интерфейс (простота) или `ApplicationDescriptor` со статическими callback + userData (минимальные
зависимости и лучшая тестируемость). На embedded полезно предусмотреть второй вариант без heap и RTTI, а высокоуровневую
C++-обёртку сделать тонкой.

### 4.2. Пример `app_main()` конечной прошивки

```cpp
#include <lexxos/ce/host.h>
#include "home_lights_application.h"

extern "C" void app_main() {
    static lexxos::ce::Host host;
    static home::HomeLightsApplication application;

    const auto status = host.initialize({});
    if (!status.ok()) {
        home::enterSafeFailureMode(status);
        return;
    }
    if (!host.registerApplication(application).ok() || !host.start().ok()) {
        home::enterSafeFailureMode();
        return;
    }

    // Политика app_main task явная: либо возвращение допустимо в
    // выбранном IDF entrypoint, либо vTaskDelete(nullptr) здесь.
    // LexxCE НЕ должна тайно удалять task в initialize().
}
```

`enterSafeFailureMode` обязан приводить выходы в заранее установленное состояние без доступа к ещё неинициализированным
сервисам. Ни один API здесь пока не реализован.

### 4.3. Режимы доступа к GPIO

Предложить **типизированный сервис**:

```cpp
// Концептуальные сигнатуры
InputHandle  openInput("switch.living", InputOptions{...});
OutputHandle openOutput("relay.living", OutputOptions{...});
Status      setOutput(OutputHandle&, bool logicalOn);
Subscription subscribe(InputHandle&, InputCallback, void*);
```

Требования к контракту: пользователь работает с **логическим ON/OFF**, а адаптер инвертирует physical level при
active-low реле; проверяется допустимый pin и ownership; запись возвращает статус; при отказе записи состояние`unknown`/
`fault`, а не ложное подтверждение `on`; можно заблокировать пользовательскую функцию во время безопасной
инициализации/OTA. У дескрипторов должны быть имена, физические GPIO, active level, boot-safe state, pull, debounce,
маска зарезервированных выводов, допустимые режимы.

### 4.4. Переносимость

Публичный API не должен включать `<driver/gpio.h>`, `<esp_http_server.h>` или IDF `esp_err_t`. IDF-специфичные адаптеры
живут под `ports/espidf` или `platform/espidf`; базовая логика требует только C++20 и минимальные контракты. Плата
описывается `BoardDescriptor`/`IoBoardProfile`, который не содержит бизнес-правил.

### 4.5. Совместимость со Standard

- Не менять значение существующих public enum и API без migration plan.
- После выделения CoreSystem, существующий `OperatingSystem` должен делегировать ему DeviceManager/CapabilitySet, не
  создавать второй независимый registry.
- Старые CMake presets и `lexxos::app::run()` должны продолжать работать без CE-флагов.
- Общие драйверы и исправления должны проверяться и в Standard, и в CE на CI.

---

## 5. Пример реальной поставки как статической библиотеки

### 5.1. ESP-IDF: рекомендуемый способ (source component)

```text
home-lights-firmware/
  CMakeLists.txt
  sdkconfig.defaults
  partitions.csv
  main/
    CMakeLists.txt
    app_main.cpp
  src/
    home_lights_application.cpp
    ...
  config/
    relay4_board.json  # исходник конфигурации сборки, не обязательно runtime
  # зависимость на LexxCE через EXTRA_COMPONENT_DIRS,
  # Component Manager или git submodule с закреплённым commit
```

**Предлагаемый** `CMakeLists.txt` потребителя:

```cmake
cmake_minimum_required(VERSION 3.22)
set(EXTRA_COMPONENT_DIRS
        "${LEXXOS_SOURCE_DIR}/ports/espidf/components"
)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(home_lights)
```

**Предлагаемый** `main/CMakeLists.txt`:

```cmake
idf_component_register(
        SRCS "app_main.cpp" "../src/home_lights_application.cpp"
        INCLUDE_DIRS "../src"
        REQUIRES lexxos_ce
)
```

Внутри `lexxos_ce/CMakeLists.txt` после рефакторинга:

```cmake
# СХЕМА, не готовый drop-in файл
idf_component_register(
        SRCS ${LEXXOS_CORE_SOURCES} ${LEXXOS_CE_SOURCES}
        INCLUDE_DIRS "${LEXXOS_PUBLIC_INCLUDE_DIR}"
        REQUIRES freertos esp_timer esp_driver_gpio
        PRIV_REQUIRES nvs_flash
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
```

При подключении `lexxos_ce_http` он отдельно объявляет `REQUIRES esp_http_server` и ссылается на `lexxos_ce`, чтобы
**HTTP не попадал в GPIO-only прошивку**. Статическую `.a` соберёт обычная цепочка ESP-IDF. Путь к компоненту, номера
версий и IDF dependency list в примере должны быть проверены фактической конфигурацией проекта.

### 5.2. Обычный CMake consumer (host/platform tests)

```cmake
add_subdirectory(path/to/LexxOS/ce path/to/build/lexxce)
target_link_libraries(my_app PRIVATE lexxos::ce)
```

Это **цель будущего рефакторинга**, пока `ce/CMakeLists.txt` не существует. Не делать `install(EXPORT)` с
неподконтрольным набором transitively linked SDK headers без предварительной ревизии. Желательно versioned CMake
package + `lexxos_ce_version.h`, чтобы потребители могли фиксировать совместимость.

### 5.3. Возможность предварительно собранной `.a`

Поддержать позже для конкретного `esp32` + конкретной версии ESP-IDF/toolchain/sdkconfig/feature profile, с manifest:
target triple, chip, IDF version, config hash, build commit, compiler flags, public API version. В первой итерации
поддерживать **сборку из исходников**, потому что это надёжнее при использовании разных ESP32-плат и пользовательских
board profiles.

---

## 6. Архитектура приложения управления светом

### 6.1. Разделить физику, состояние и поведение

```text
GPIO ISR (короткая операция)
    |
    v
ISR-safe queue / notification
    |
    v
InputWorker: debounce + normalizing + timestamp
    |
    v
GestureRecognizer: press/release/long/double
    |
    v
RuleEngine: declarative conditions + priority
    |
    v
CommandDispatcher: owner of mutable lighting state
    |
    +--> OutputDriver: active-low/high, limits, fault
    +--> EventPublisher: snapshot / sequence / diagnostics
    +--> ConfigStore: delayed, controlled persistence

REST handlers ---> queue of Commands ---> тот же CommandDispatcher
```

**Единственный источник истины о логическом состоянии выходов** — `LightController`/`CommandDispatcher`. Ни HTTP
handler, ни ISR не переключают GPIO напрямую. События и команды не эквивалентны: событие — факт, команда — намерение
выполнить действие. Это предотвращает гонки «короткое нажатие одновременно с REST PUT».

### 6.2. Минимальные типы предметной области

```cpp
using InputId = std::uint8_t;
using OutputId = std::uint8_t;
using GroupId = std::uint8_t;

enum class Gesture : std::uint8_t {
    Press, Release, ShortPress, LongPress,
    LongRelease, DoublePress
};

enum class ActionType : std::uint8_t {
    Set, Toggle, Pulse, TurnOthersOff, ToggleOthersOn,
    RestoreGroup, ApplyScene, Lock, Unlock
};

struct OutputState {
    bool requestedOn{false};
    bool confirmedOn{false};  // только если hardware feedback доступен
    bool known{false};        // без feedback это «команда отправлена», не подтверждение реле
};
```

Для платы с обычными реле **нельзя** утверждать аппаратное `confirmedOn`, если на плате нет обратной связи с контактов.
Предлагается различать `commanded_state`, `gpio_level` и `feedback_state` (optional), иначе REST API будет показывать
ложную гарантию включения света.

### 6.3. Обработка нажатий

Состояния каждого входа:
`StableInactive -> DebouncingPress -> Pressed -> LongHeld -> DebouncingRelease -> StableInactive`. Часы — **монотонный
tick**, а не NTP/RTC wall clock. После срабатывания `LongPress` на пороге длительности **не** генерировать `ShortPress`
при отпускании. События подавлять и фильтровать по physical edge, не по частоте опроса веб-сервера.

**Рекомендуемые стартовые значения**, настраиваемые для каждой платы и каждого входа: debounce 25–40 мс, long press
700–1000 мс, double press window 250–400 мс, минимальная длительность импульса 30–50 мс. Это инженерные начальные
параметры, **не результаты измерений** конкретных кнопок. Контроллер не должен блокировать входной task на всю
длительность удержания: используется state machine + timer deadline.

**Особенности:**

- если включён `DoublePress`, `ShortPress` откладывается до окончания double window; иначе short обрабатывается сразу;
- если кнопка уже зажата при запуске, политику определить явно (`ignore_until_release` предпочтительнее случайного long
  press при включении);
- событие длинного удержания создаётся **один раз** до отпускания, а повторение — только если это отдельная настройка;
- переполнение очереди и потерянный release не должны оставлять виртуальную кнопку «навечно удержанной»: предусмотрены
  resync текущего GPIO и счётчик ошибок;
- для механических входов выключателей положение `latching switch` обрабатывается отдельно от `momentary button`;
- фиксировать последовательность событий и приоритет между short/double/long/repeat;
- короткие импульсы ниже debounce порога игнорировать; поведение при chatter тестировать на host.

### 6.4. Конфигурация правил, а не жёстко закодированные условия

Для первой версии — **таблица декларативных правил**, не произвольный Python/Lua/C++ из HTTP. Одно правило описывает
`trigger`, `condition`, `action`, `target`, `priority`, `enabled`. Действия должны быть ограниченными, проверяемыми,
детерминированными.

Пример конфигурации (иллюстрация схемы, не существующий парсер):

```json
{
  "schema_version": 1,
  "board": "esp32_relay4",
  "inputs": [
    {
      "id": 0,
      "name": "living_button",
      "mode": "momentary",
      "active_level": "low",
      "debounce_ms": 30,
      "long_press_ms": 800
    }
  ],
  "outputs": [
    {
      "id": 0,
      "name": "living_light",
      "active_level": "low",
      "boot_state": "off"
    },
    {
      "id": 1,
      "name": "kitchen_light",
      "active_level": "low",
      "boot_state": "off"
    },
    {
      "id": 2,
      "name": "hall_light",
      "active_level": "low",
      "boot_state": "off"
    },
    {
      "id": 3,
      "name": "bedroom_light",
      "active_level": "low",
      "boot_state": "off"
    }
  ],
  "groups": [
    {
      "id": "all_lights",
      "outputs": [
        0,
        1,
        2,
        3
      ]
    }
  ],
  "rules": [
    {
      "id": "living_short",
      "when": {
        "input": 0,
        "gesture": "short_press"
      },
      "do": {
        "action": "toggle",
        "output": 0
      }
    },
    {
      "id": "living_long",
      "when": {
        "input": 0,
        "gesture": "long_press"
      },
      "do": {
        "action": "toggle_others",
        "group": "all_lights",
        "exclude": [
          0
        ],
        "restore_mode": "all_on"
      }
    }
  ]
}
```

Для `toggle_others` нужна точная спецификация:

1. Первый long press при обычном режиме — выключить все выходы группы **кроме 0**; сохранить, если необходимо, snapshot
   других выходов до операции.
2. Второй long press — включить **все** другие выходы (`restore_mode=all_on`, буквально по заданному сценарию).
   Альтернативный `restore_mode=previous` восстанавливает сохранённые состояния. Если snapshot недоступен после
   перезагрузки, fallback выбирается конфигурацией.
3. После ручного или REST-изменения группы определить, сбрасывается ли «phase» переключения. Рекомендуется сбрасывать
   phase при внешнем изменении, чтобы режим был предсказуемым.
4. Активность «два удержания подряд» принадлежит **контексту конкретного правила/группы**, а не одному глобальному
   boolean для всех кнопок.
5. Правила при одновременном срабатывании сортируются по приоритету, далее по устойчивому rule-id; изменения выходов
   применяются в одной транзакции логического состояния.

### 6.5. Другие предусмотренные действия

| Тип                  | Пример                                     | Особенность                              |
|----------------------|--------------------------------------------|------------------------------------------|
| `toggle`             | короткое нажатие → инвертировать канал     | атомарно относительно REST               |
| `set_on` / `set_off` | включить/выключить один или группу         | идемпотентно                             |
| `pulse`              | замкнуть выход на 250 мс                   | таймер, лимит максимального импульса     |
| `follow_input`       | удерживается контакт → выход активен       | отдельно от кнопочного режима            |
| `delayed_off`        | выключить через 10 минут                   | таймер перезапускаем по политике         |
| `double_press`       | включить сцену                             | с окном распознавания                    |
| `group_except`       | выключить всё, кроме локального            | поддержка exclude                        |
| `scene`              | несколько каналов в заданные состояния     | пакетность применения                    |
| `interlock`          | только один из взаимно исключающих выходов | fail-safe для исполнительных схем        |
| `schedule`           | сценарий по времени                        | **позже**, требует валидных RTC/NTP и TZ |
| `condition`          | только если выбранный выход уже включён    | строгая модель порядка и состояния       |

Сценарии не должны бесконечно порождать друг друга: limit execution depth, max commands per tick, идентификатор
causation, запрет неконтролируемой рекурсии. Для REST и кнопок использовать общий `CommandDispatcher`, а не два
независимых обработчика.

### 6.6. Потоки задач на ESP32

Первый простой вариант:

- `io_worker`: высокий прикладной приоритет, очередь edge событий, debounce, deadlines, локальная state machine;
- `controller`: единственный владелец логического состояния, обрабатывает команды от кнопок/REST, публикует snapshots;
- задачи Wi-Fi/lwIP/httpd создаются ESP-IDF и **не** вызывают GPIO напрямую;
- `settings_worker` либо deferred write в контексте controller, либо отдельный низкоприоритетный исполнитель NVS с
  объединением частых изменений.

Объединить `io_worker` и `controller` в одну задачу для 2–4 кнопок можно и часто полезно. Выбор зависит от гарантии
обработки очереди и простоты shutdown; нежелательно создавать task на каждую кнопку или реле. Максимальная задержка
локальной реакции должна быть измерена под нагрузкой HTTP/NVS/Wi-Fi, а не предполагаема.

### 6.7. Надёжность контроллера — обязательные правила

- **Состояние пинов при включении:** до инициализации C++ и ОС реле должны иметь аппаратные pull-up/pull-down и
  предсказуемую полярность. Программный `boot_state` сам по себе не устраняет импульс на старте.
- **Запрещённые GPIO:** для каждой ревизии платы проверить strapping pins, flash-connected pins, input-only pins и
  занятую периферию; текущей численной проверки `0 <= pin < max` недостаточно.
- **Active-low:** инициализировать сразу с **безопасным физическим уровнем**, без промежуточного glitch; проверить
  порядок output latch vs direction.
- **Ошибки конфигурации:** если JSON/NVS повреждён, загружать проверенный fallback, не «угадывать» назначение выходов;
  несовместимый pin map — отказ запуска automation и safe outputs.
- **Watchdog:** следить за зависшими обработчиками, но не перезапускать устройство при каждом обрыве Wi-Fi; должен быть
  диагностируемый reset reason.
- **Brownout / восстановление:** после пропадания питания выбираемая политика `all_off`/`restore_last`/`fixed` с учётом
  безопасности и ресурса ламп; не делать запись NVS на каждое нажатие.
- **Питание/реле:** поддержать отдельно обычные и бистабильные реле, импульс на coil, задержку между направлениями и
  максимальную длительность активации.
- **Электрическая безопасность:** коммутация сетевого напряжения требует соответствующей изоляции, сертифицированных
  компонентов, защитных элементов и аппаратного тестирования. Прошивка не заменяет эти требования.

---

## 7. REST API для удалённого управления

**Предлагаемый backend:** `esp_http_server` от ESP-IDF через отдельный адаптер `lexxos_ce_http`, а не собственная
реализация HTTP. HTTP server предоставляет handlers, но **не гарантирует потоко-безопасность прикладного состояния**;
обработчики публикуют команды в очередь контроллера и получают результат/снимок состояния через определённый protocol.
REST не должен создавать прямой второй путь к GPIO.

### 7.1. Версионирование и endpoints

| Метод  | URL                              | Назначение                                           |
|--------|----------------------------------|------------------------------------------------------|
| `GET`  | `/api/v1/system`                 | версия, uptime, board, capabilities, reset reason    |
| `GET`  | `/api/v1/inputs`                 | конфигурация входов, текущие raw/debounced состояния |
| `GET`  | `/api/v1/outputs`                | логические состояния и last command result           |
| `GET`  | `/api/v1/outputs/{id}`           | канал и status/sequence                              |
| `PUT`  | `/api/v1/outputs/{id}/state`     | идемпотентно установить `on/off`                     |
| `POST` | `/api/v1/outputs/{id}/toggle`    | toggle с `request_id`/защитой от повторов            |
| `POST` | `/api/v1/scenes/{name}/activate` | активировать сцену                                   |
| `GET`  | `/api/v1/rules`                  | текущие правила                                      |
| `PUT`  | `/api/v1/config`                 | validate + atomic swap конфигурации                  |
| `GET`  | `/api/v1/config`                 | показать redacted настройки                          |
| `GET`  | `/api/v1/health`                 | безопасная диагностика, ошибки очередей/выходов      |
| `GET`  | `/api/v1/events`                 | опционально SSE/long polling, **позже**              |

Не реализовывать удалённый shell или API произвольного выполнения кода. Режим provisioning (настройка Wi-Fi и секретов)
должен быть отдельным, ограниченным во времени и доступным лишь после физического действия или другой надёжной процедуры
авторизации.

### 7.2. Пример запроса/ответа

```http
PUT /api/v1/outputs/2/state
Authorization: Bearer <device-token>
Content-Type: application/json
If-Match: "cfg-19"

{"on":true,"request_id":"9d85e2c6"}
```

```json
{
  "output_id": 2,
  "requested_on": true,
  "applied_gpio_level": false,
  "hardware_feedback": "unavailable",
  "state_sequence": 183,
  "result": "applied"
}
```

`If-Match` здесь пример optimistic concurrency для конфигурации; в API для *состояния выхода* надо определить отдельный
`state_revision` или не использовать условие — не смешивать эти версии. HTTP codes: 200/202 success, 400 schema, 401/403
auth, 404 channel, 409 conflict, 412 stale revision, 422 validation, 429 rate limit, 503 unavailable/queue full. Лимиты
размера тела (например ≤ 4–8 КБ для config MVP), таймауты и числа соединений задаются с учётом RAM.

### 7.3. Базовые требования безопасности

1. Все write endpoints и чтение конфигурации требуют аутентификации; токен случайный, индивидуальный для устройства,
   хранится как секрет, не выдаётся в GET и не пишется в лог.
2. HTTP только в доверенной локальной сети — как минимум отдельный VLAN/firewall и отсутствие проброса порта в интернет.
   Для недоверенной сети потребуется TLS на устройстве или аутентифицированный reverse proxy/VPN с учётом всей цепочки
   доступа.
3. Включить ограничение частоты, числа соединений, времени обработки, длины заголовков/тела, проверку `Content-Type`,
   корректные ошибки без утечки секретов.
4. Если будет browser UI: не полагаться только на CORS; учесть CSRF при cookie-based auth, не использовать cookies как
   единственную защиту.
5. Device discovery/mDNS — опционально; не публиковать secrets; подтверждать ownership/provisioning.
6. Токены и параметры Wi-Fi не включать в экспорт диагностических snapshot.
7. Для обновлений — подписанная/проверенная прошивка, HTTPS OTA, возможность rollback после самотеста, аппаратная
   recovery-процедура.

**Рекомендация:** сначала REST только в локальной сети, включать расширенные интеграции (MQTT/Home Assistant) вторым
этапом. Это не блокирует локальные физические сценарии.

---

## 8. Конфигурация, NVS и восстановление

### 8.1. Три разных набора данных

1. **Board hardware profile** — назначение GPIO, допустимые режимы, active level, safe boot states; лучше фиксировать в
   прошивке или доверенной подписанной конфигурации. Пользовательский REST не должен произвольно назначать реле на
   boot/flash GPIO.
2. **Logic configuration** — правила, сцены, тайминги, группы, пользовательские имена, версия схемы; хранить в NVS с
   проверкой на целостность и совместимость.
3. **Runtime state** — последнее логическое состояние, временные timers, phase группы. Не всё нужно сохранять. Состояние
   реле сохранять только при реальной необходимости и ограничивать flash wear.

### 8.2. Атомарное обновление

`PUT /config` выполняет
`parse -> schema validate -> board capability validate -> conflict checks -> build immutable new config -> stage in NVS -> commit -> atomic swap to live state -> ack`.
При сбое записи предыдущая проверенная конфигурация должна остаться доступной. Конкретная схема может быть двухслотовой
(`cfg_a`/`cfg_b`, generation/CRC, active selector), но следует учитывать реальные гарантии NVS и её partition capacity;
не считать две записи в одном namespace полноценной произвольной транзакцией без теста power-loss. Ввести
`schema_version`, `generation`, `crc`, `last_good` и migration функции.

### 8.3. Управление износом flash

NVS подходит для небольших, относительно редко меняющихся конфигураций, а не постоянного журнала каждого переключения.
Для состояния: debounce writes (например только по idle-window и не чаще заданного интервала), опциональный
периодический snapshot и явная политика потери последних изменений при power loss. Для критичной механики у реле
предпочтительнее безопасный старт, чем частые NVS writes.

### 8.4. Восстановление после сбоя

Алгоритм `boot`:

1. Выставить безопасные уровни GPIO как можно раньше.
2. Проверить board descriptor и ограничения пинов.
3. Инициализировать NVS, выбрать последнюю **валидную** конфигурацию.
4. При невалидной: fallback board defaults + diagnostics, при необходимости запретить удалённое управление до
   исправления.
5. Создать управление выходами, состояние групп/правил.
6. Активировать локальное управление и только затем сетевые сервисы.
7. OTA image подтверждать как рабочую после базовых проверок критических локальных функций, не требуя доступности
   внешнего Wi-Fi.

---

## 9. Сеть, время, OTA и дополнительные интеграции

### 9.1. Сетевой слой

ESP-IDF Wi-Fi + lwIP оставить backend; `NetworkService` в CE предоставляет
`start()/stop()/status()/subscribeConnectionChanges()`, не раскрывая IDF события в публичном переносимом API.
Подключение к Wi-Fi выполняется асинхронно, с ограниченным exponential backoff. Локальные кнопки работают независимо от
IP и REST.

На следующем этапе отдельно рассмотреть `MQTT` и `Home Assistant discovery`, потому что это даст готовую интеграцию без
повторной реализации rule engine в сторонней автоматизации. MQTT — **не обязательная зависимость** core или MVP.

### 9.2. Время

Для жестов, задержек и watchdog всегда monotonic clock (в ESP-IDF — подходящий monotonic time source/timer). RTC/NTP
нужны только для правил «в 22:00» и истории с wall timestamps. При отсутствии доверенного времени schedules должны
переходить в понятный `time_unavailable`, а не срабатывать в 1970 году.

### 9.3. OTA

После MVP: опциональный `lexxos_ce_ota` поверх `esp_https_ota`, две OTA partitions, подпись/проверка, самооценка
startup, откат при неработоспособной прошивке. Совместимость schema migration и OTA rollback нужно решать совместно:
новая прошивка не должна необратимо переписать конфигурацию так, что старая перестанет запускаться. Применять staged
migrations либо dual-read policy. Во время OTA локальные кнопки и безопасность выходов остаются приоритетными;
ограничить длительные blocking calls в критичных задачах.

---

## 10. Тестируемость и критерии приёмки

### 10.1. Host-тесты без ESP32

Вынести `GestureRecognizer`, `RuleEngine`, `GroupSceneController`, `ConfigValidator`, `OutputStateMachine` в
platform-neutral C++20 модуль с fake clock, fake GPIO и fake NVS. Использовать существующий каталог `tests/cpp` и
установленную инфраструктуру CTest. Проверять:

- debounce с дребезгом, very short press, long press, hold/release, double press;
- переполнение ISR queue и resync;
- одновременные команды от кнопки и REST, строгий order и отсутствие двойного toggle;
- действия group except, phase-on/phase-off, restore previous vs all_on, внешние изменения группы;
- конфликтующие правила, priorities, non-reentrant events, bounded recursion;
- ошибки GPIO write, потерю hardware feedback, ошибки NVS, повреждённые JSON/CRC;
- сериализацию/миграцию versioned configuration, invalid pin mapping, restart во время применения;
- сценарии отключённого Wi-Fi, недоступного NTP, рестарта сети и смены конфигурации на лету.

### 10.2. Матрица сборок

| Проверка               | Platform           | Edition  | Результат                                |
|------------------------|--------------------|----------|------------------------------------------|
| Полная текущая система | ESP-IDF ESP32      | Standard | build/link + smoke, без регрессий        |
| Полная текущая система | ESP-IDF ESP32-S3   | Standard | build/link + smoke                       |
| Минимальная embedded   | ESP-IDF ESP32      | CE tiny  | без shell/VFS/program sources в link map |
| Умный дом              | ESP-IDF ESP32      | CE iot   | GPIO + Wi-Fi + HTTP + NVS                |
| Проверки core и правил | native macOS/Linux | CE host  | ASan/UBSan/CTest                         |
| По мере готовности     | Pico/STM32         | CE tiny  | тот же app API, другой platform adapter  |

### 10.3. Аппаратные тесты (обязательны перед реальным светом)

- 1000+ циклов коротких и длинных нажатий под нагрузкой REST и Wi-Fi reconnect; детектировать неверные двойные события.
- Многократный power-cycle на выключенном/включенном реле: не должно быть непредусмотренных переключений или boot
  glitches.
- Не менее одного контролируемого power-loss при записи конфигурации с последующей успешной загрузкой последней
  целостной версии.
- 24–72 часа soak-test с периодическим REST, Wi-Fi drop/reconnect и непрерывным сбором watermark/heap/error counters
  (длительность ориентировочная, не гарантия надёжности).
- Проверить кнопку при загрузке, кнопку одновременно с reboot и обрыв Wi-Fi в середине операции.
- Измерить latency: GPIO edge → команда применена → GPIO output; p50/p99/max; заранее зафиксировать SLO, например p99 до
  100 мс **после debounce** для локального действия при допустимой нагрузке (предварительная цель, уточнить на железе).
- Подтвердить boot strap stability, active-low inversion, реле с оптопарами/транзисторами, electrical isolation и power
  failure behavior.

### 10.4. Инструментирование

Добавить `GET /health` и локальные счётчики: `input_events`, `debounce_drops`, `event_queue_overflows`,
`rule_rejections`, `output_write_failures`, `nvs_commit_failures`, `http_command_timeouts`, `heap_free/min`,
`task_stack_high_water`, `last_reset_reason`, `uptime`, `last_boot_result`.

**Важный критерий:** наличие endpoint не означает, что метрики корректны. Перед публикацией исправить байтовую семантику
минимального свободного остатка стека ESP-IDF.

---

## 11. Пошаговый план разработки

### Этап 0. Техническая стабилизация ESP-IDF backend — P0

**Задачи:** исправить stack size и watermark в `scheduler_espidf.cpp`; проверить `EventBus` при `LEXXOS_KERNEL_ESPIDF`;
защитить multi-core task slots; формализовать ISR-safe события и GPIO. Провести build и runtime smoke Standard на
существующих ESP32-платах.

**Готовность:** нет расхождения запрошенного/реального размера стека; ISR publication безопасен; Standard не сломан.

### Этап 1. Выделить shared CoreSystem и bootstrap — P0

**Задачи:** отделить `DeviceManager`, `PlatformServices`, `CapabilitySet`, `scheduler bind`, `EventBus` от
shell/VFS/program manager; создать `CoreContext` и bootstrap без обязательного terminal/SD; исключить unconditional
`AppContext` dependencies; добавить unit tests.

**Готовность:** host minimal test вызывает CoreSystem init, регистрирует mock GPIO, получает capabilities, не включает
shell headers в CE public interface.

### Этап 2. Реализовать CE build target и minimal example — P0

**Задачи:** реорганизовать source groups, сделать `LEXXOS_EDITION=ce`, добавить `lexxos_ce` ESP-IDF component и пример
`minimal_blink`. Проверить, что `idf_component_register` не компилирует `shell.cpp`, `runtime_tasks.cpp`,
`external_program_runner.cpp`, `gameboy/*`, `vfs/*` для `tiny`.

**Готовность:** отдельный проект компилирует собственный `app_main.cpp`, статически подключает CE, запускает GPIO без
LexxOS shell.

### Этап 3. Board profile и безопасные входы/выходы — P0

**Задачи:** `esp32_relay2`/`esp32_relay4`, pin capabilities, active-low, startup safe levels, `GpioLease`, асинхронный
InputWorker с debounce/edge handling, OutputController, fault reporting.

**Готовность:** 2/4 входа и реле работают независимо от Wi-Fi; физический pin map validated.

### Этап 4. Правила освещения и scenes — P0

**Задачи:** GestureRecognizer, RuleEngine, group actions, long-press two-phase behavior, priority, fake-time tests,
атомарное состояние команд. Сначала неизменяемая прошитая конфигурация для быстрой проверки поведения.

**Готовность:** сценарии из §1.1 и все tests из §10.1 проходят на host и ESP32.

### Этап 5. REST и конфигурация NVS — P1

**Задачи:** `lexxos_ce_http`, `ConfigStore`, JSON schema/validator, versioned REST, auth, rate limiting, atomic apply,
migration, Wi-Fi reconnect. Настройки меняются без перепрошивки, **но физические ограничения платы остаются
защищёнными**.

**Готовность:** REST и физическое нажатие используют один dispatcher; правила сохраняются после reboot, corrupt config
безопасно отклоняется.

### Этап 6. Эксплуатация и интеграция — P1/P2

**Задачи:** OTA rollback, factory reset с физическим подтверждением, monitoring, optional MQTT/HA discovery, extended
шаблоны (взаимоблокировки, расписание, сцены), версии сборок и пакетов.

**Готовность:** стабильная эксплуатация, восстановление, воспроизводимые сборки и документированные ограничения.

### 11.1. Рекомендуемая разбивка на PR/патчи

| Патч  | Суть                                                 | Не смешивать с                |
|-------|------------------------------------------------------|-------------------------------|
| CE-00 | ESP-IDF scheduler/event bus correctness              | бизнес-логикой света          |
| CE-01 | `CoreSystem` + `CoreContext` и тесты                 | переносом shell               |
| CE-02 | CMake source groups, edition switch, `minimal_blink` | новой сетью                   |
| CE-03 | Владение GPIO и безопасная инициализация выходов     | REST                          |
| CE-04 | InputGesture, движок правил и тесты на компьютере    | HTTP, специфичный для ESP-IDF |
| CE-05 | `home_lights` ESP32 board app                        | универсальной rule-VM         |
| CE-06 | NVS, REST и безопасность                             | OTA                           |
| CE-07 | OTA + monitoring + compatibility                     | UI редактором                 |

**Предпочтительный порядок:** выделить минимальный Core до внедрения REST. Не пытаться сначала собрать «всё в одну `.a`»
и только затем удалять лишние зависимости.

---

## 12. Размер и производительность: как оценивать, а не гадать

Конкретные размеры RAM/flash в проверенной ревизии **не измерялись**. Для первых сборок отчёт должен включать
`idf.py size`, `idf.py size-components`, linker map и метрики heap before/after HTTP init. На ESP32 с ограниченной RAM
фиксировать бюджет **по компонентам**:

| Статья             | Что измерять                       | Возможный оптимизационный подход                         |
|--------------------|------------------------------------|----------------------------------------------------------|
| Core/DeviceManager | `.bss`/`.data`, init heap          | CE профиль ограничений, исключение registry лишних типов |
| Event queues       | capacity × размер события          | компактные domain events, telemetry overflow             |
| Input/Controller   | stack + high-water                 | 1–2 task, без task на каждый вход                        |
| Wi-Fi/lwIP         | internal heap, buffers             | конфиг ESP-IDF, ограничение соединений                   |
| HTTP               | server task stack, request buffers | небольшой max body, bounded handlers                     |
| NVS                | записи flash / размер раздела      | объединение записей, ограниченный объём конфигурации     |
| Diagnostics        | compiled strings/log buffers       | уровни, stripped debug в production                      |

Сохранять минимальную прошивку без лишнего HTTP/NVS для устройств, где требуется только local GPIO. Если `CE iot`
укладывается в бюджет на реальной ESP32, не заниматься преждевременной ручной заменой стандартных библиотек ESP-IDF.

---

## 13. Риски и проектные компромиссы

| Риск                                       | Последствие                           | Мера                                                  |
|--------------------------------------------|---------------------------------------|-------------------------------------------------------|
| CE дублирует Core                          | два расходящихся ядра                 | одна реализация, два composition paths                |
| CE компилирует Standard runtime            | не достигается минимальность          | строгий source split + link-map CI                    |
| Неправильная семантика ESP-IDF stack units | stack overflow/ложная метрика         | P0 исправление и stress                               |
| ISR и dual-core гонки                      | потеря событий, нестабильный свет     | ISR-safe queue, один controller owner                 |
| GPIO boot glitches                         | случайное включение реле              | электрические pull, boot-safe hardware profile        |
| Прямое управление из REST                  | двойное переключение и race           | сериализованный command dispatcher                    |
| Сложный rule engine изначально             | сложные тесты, непредсказуемость      | declarative bounded rules, без scripting MVP          |
| Частая запись NVS                          | износ flash                           | batching, commit policy, power-loss testing           |
| Небезопасный API в LAN                     | несанкционированное управление светом | токены, сегментация, limits, OTA security             |
| Нарушение rollback config                  | устройство не стартует после OTA      | versioned schema, reversible migration                |
| Неподходящий универсальный `.a`            | ABI/linker ошибки                     | source component, фиксированный manifest binary later |
| Завязка на ESP-IDF типов                   | не переносится на Pico/STM32          | платформенно нейтральные интерфейсы и host tests      |

---

## 14. Определение готовности LexxCE v0.1

Версия `v0.1` считается готовой, если:

- [ ] `LexxCE` собирается самостоятельным ESP-IDF component в другом проекте, приложение имеет собственный`app_main`.
- [ ] Публичный CE API не включает shell, VFS и ESP-IDF hardware types.
- [ ] Standard LexxOS на прежних board presets успешно собирается и проходит smoke tests.
- [ ] `minimal_blink` включает и выключает GPIO через зарегистрированный device resource, не напрямую из shell.
- [ ] На целевой ESP32-плате надёжно работают 2–4 входа и 2–4 выхода с аппаратно проверенным safe startup.
- [ ] Короткое нажатие делает toggle, long press меняет другие каналы по явно заданной фазе, повторный long press
  работает по политике `all_on`/`previous`.
- [ ] Wi-Fi/HTTP не влияют на локальную работу контроллера, даже при reconnect.
- [ ] REST запросы не обращаются к GPIO напрямую, проходят auth и dispatcher.
- [ ] Настройки правил валидируются, сохраняются и восстанавливаются; повреждение конфигурации безопасно обрабатывается.
- [ ] Исправлены проблемы с ESP-IDF task stack units, ISR synchronization и task slot races.
- [ ] Есть host tests с fake time/input/output, build matrix, измеренные size/RAM и хотя бы один hardware soak test.

### Рекомендуемый следующий конкретный шаг

**Сначала подготовить патч CE-00 + CE-01**, не касаясь логики умного дома: исправить критичные особенности ESP-IDF,
выделить `CoreSystem` и `CoreContext`, сохранить обычный `Runtime`, добавить контрактные host-тесты. Это создаст
правильную основу, после чего независимый проект `home_lights` сможет линковаться с `lexxos_ce` и не будет зависеть от
реализации shell.

---

## 15. Источники и точные точки кода

### Исходники LexxOS

- [
  `src/lexxos/os/operating_system.h`](../src/lexxos/os/operating_system.h).
- [
  `src/lexxos/os/app_context.cpp`](../src/lexxos/os/app_context.cpp).
- [
  `src/lexxos/app/runtime.h`](../src/lexxos/app/runtime.h), [
  `runtime.cpp`](../src/lexxos/app/runtime.cpp), [
  `runtime_tasks.cpp`](../src/lexxos/app/runtime_tasks.cpp).
- [
  `config/cmake/lexxos_sources.cmake`](../config/cmake/lexxos_sources.cmake), [
  `Features.cmake`](../config/cmake/Features.cmake).
- [
  `ports/espidf/components/lexxos/CMakeLists.txt`](../ports/espidf/components/lexxos/CMakeLists.txt), [
  `espidf_main.cpp`](../ports/espidf/components/lexxos/espidf_main.cpp).
- [
  `src/lexxos/kernel/espidf/scheduler_espidf.cpp`](../src/lexxos/kernel/espidf/scheduler_espidf.cpp).
- [
  `src/lexxos/lifecycle/event_bus.cpp`](../src/lexxos/lifecycle/event_bus.cpp).
- [
  `src/lexxos/platform/platform_services.h`](../src/lexxos/platform/platform_services.h), [
  `src/lexxos/platform/espidf/detail/espidf_gpio.cpp`](../src/lexxos/platform/espidf/detail/espidf_gpio.cpp).
- [
  `src/lexxos/devices/device_manager.h`](../src/lexxos/devices/device_manager.h), [
  `src/lexxos/runtime/runtime_profile.h`](../src/lexxos/runtime/runtime_profile.h).
- [
  `src/lexxos/composition/board_device_providers.cpp`](../src/lexxos/composition/board_device_providers.cpp).

### Документация ESP-IDF

- [Система сборки ESP-IDF /
  `idf_component_register`](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html).
- [FreeRTOS (IDF): размер стека в байтах](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/system/freertos_idf.html).
- [Память ESP32 и остаток стека в байтах](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/performance/ram-usage.html).
- [HTTP-сервер ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_server.html).
- [ESP-IDF NVS](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html).
- [ESP HTTPS OTA](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_https_ota.html)
  и [OTA rollback](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html).
- [ESP32 strapping pins: аппаратные ограничения boot](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32/schematic-checklist.html).

---

*Исходное предложение от 4 ноября составлено без изменения репозитория.
Редакционная подготовка документации от 5 ноября не является реализацией LexxCE.*
