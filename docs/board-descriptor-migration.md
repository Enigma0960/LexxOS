# BoardDescriptor: identity и совместимость

Этот этап выделяет из общего контракта только идентичность платы. Он не
объявляет завершённой миграцию всей периферии.

## Контракты

- `platform::BoardDescriptor`: только `boardId`, `boardName`, `vendor`,
  `processorFamily` и `processorName`. Не содержит маршрутов, драйверов,
  GPIO, периферийных конфигураций и runtime policy.
- `platform::LegacyBoardDescriptor`: временная проекция существующего API
  для действующих платформ, providers и diagnostics. Поля и значения не
  изменяются; новые аппаратные описания не должны добавляться в этот тип.
- `platform::boardIdentity(legacy)`: constexpr snapshot identity; ссылка
  на строки живёт столько же, сколько исходная декларация.
- `hardware/modules`: типизированный перечень физических контроллеров
  и возможностей модуля.
- `hardware/products/*wiring.h` и `*endpoints.h`: связи компонентов,
  выбор интерфейсов и проверка конфликтов.
- `hardware/products/*policy.h`: product-политики storage и иных устройств;
  stage-политики выбираются в composition, не в identity.
- `hardware/legacy`: только проекция существующего hardware model в
  `LegacyBoardDescriptor`, без дублирования конфигурации как источника истины.

## Ограничения текущего этапа

Lyra/PicoCalc уже использует `LyraProduct`, `LyraEndpoints`,
`PicoCalcLyraWiring` и `StoragePolicy`. Для Pico, STM32, ESP-IDF и
эмулятора ещё требуется вынести источник периферийных данных в отдельные
компонентные декларации и создавать legacy projection из них. До этого
`LegacyBoardDescriptor` сохраняется, его удаление преждевременно.

Сохранены значения, порядок инициализации, storage defaults и feature flags.
Изменение имени типа в legacy callers — запланированное source API migration
внутри репозитория. Внешние потребители исходного C++ имени должны обновить
включаемые интерфейсы; ABI не объявляется совместимым.

## Проверка

1. Проверить `git apply --check` и `git diff --check`.
2. Собрать host contract tests и simulator; повторить Pico, ESP-IDF, STM32,
   RK3506 presets по мере доступности toolchains.
3. Проверить UART/SD/LCD/I2C и FreeRTOS startup на реальной Lyra; host
   проверка не заменяет hardware acceptance.
4. До удаления legacy adapter перевести все consumers на типизированные
   module/component declarations и explicit product/stage policies.

По чеклисту временный legacy bridge является документированным исключением,
а окончательная миграция потребителей пока не завершена.


## Шаг 2: dev_pico

- `platform/board_identity.h` — изолированный API идентичности.
  `platform/board.h` временно включает legacy для обратной совместимости;
  рекурсивная зависимость заголовков устранена.
- Декларации интерфейсов RP2040, SPI-устройств, PCF8575 и Sega отделены;
  physical wiring хранится в `hardware/products/dev_pico_*.h`.
- Частоты/тайминги — product policy, SPI clock экрана — stage policy.
- Только `hardware/legacy/dev_pico_board.h` создаёт прежнюю проекцию.
  Старые значения пинов и defaults сохранены, в том числе `madctl`.
- Остальные платформы пока работают через `LegacyBoardDescriptor`.
  Для них требуется поэтапная миграция на декларации компонентов.
