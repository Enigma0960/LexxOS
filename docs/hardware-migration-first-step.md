# Переход к новой аппаратной архитектуре: состояние и оставшиеся задачи

Актуально на 29 октября 2026. Миграция продолжается: частично реализованы
HW-2/HW-3/HW-4/HW-7, начата интеграция FreeRTOS в отдельной программе KRN.
Ни один большой этап не считается принятым только по наличию кода или host-тестов.

Выполненные изменения и результаты промежуточных проверок перенесены в
[черновик release notes 0.1.1](release-0.1.1.md). Этот файл заменяет прежний
хронологический журнал первого переноса; его путь сохранён для действующих ссылок.
Статусы этапов ведутся в [roadmap](../roadmap.md), целевые зависимости и критерии —
в [плане HW](hardware-boot-architecture-plan.md) и
[плане KRN](armv7a-irq-dma-kernel-roadmap.md).

## Текущие границы ответственности

| Область         | Текущая граница                                                                               | Что ещё связывает слои                                                         |
|-----------------|-----------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------|
| Hardware data   | Modules/wiring → проверенные endpoints по значению → composition; отдельная legacy projection | Неполные routes/revisions/power dependencies; legacy callers                   |
| Composition     | Выбор providers, SD service, SPI queues, LCD backend/workspaces/controller                    | Часть раннего init и native transport остаются глобальными                     |
| Device core     | ILI9488 commands/pixels/stream и PL330 encoder получают зависимости явно                      | Нет общего U-Boot/early применения и второго проверенного backend              |
| Display service | Два кадра, dirty region, presenter, внедряемая синхронизация                                  | Один producer/CPU0; большие фиксированные workspace вне единого memory budget  |
| RK3506 backend  | MMIO/SPI/DDR/IRQ/drain, фиксированный DMAC0 owner                                             | Legacy API, общий recovery и lifecycle ещё не оформлены целиком                |
| SD service      | Policy snapshot, transport/filesystem interfaces, контекст hooks                              | Native DMA/GPIO/time bridge, отдельные SD protocol paths, общий FatFs registry |
| Kernel          | Cooperative и экспериментальный FreeRTOS профиль                                              | IRQ/preemption audit и аппаратная приёмка; DMA в FreeRTOS отключён             |

## Ближайшие работы

1. Продолжить перевод раннего init и native adapters на разрешённые hardware endpoints;
   остаются миграция legacy diagnostic callers на переданный экземпляр протокола
   и отделение аппаратных настроек backend. SD-протокол получает SPI/DMA/GPIO/time
   через SdProtocolIo; native backend ещё связывает глобальные DMA/GPIO/time entrypoints. I2C owner уже находится в
   composition;
   совместимые I2C accessors только направляют к нему.
   Пользовательский GPIO inventory Lyra пока пуст; расширять его вместе с явной
   регистрацией и проверкой конфликтов. Ревизии, voltage и power dependencies добавлять по схеме.
   Завершить контракты ownership/lifecycle транспорта: явные controller/endpoint
   зависимости, правила quiesce/adopt/reset, порядок locks и время жизни IRQ callbacks.
   Согласовать восстановление SD/LCD после общего manager fault без переиспользования
   памяти при неподтверждённой остановке. Legacy wrappers удалять после миграции callers.
2. Перевести оставшиеся legacy diagnostic callers на явный экземпляр SD-протокола,
   оформить владельца SD DMA и объединить SD SPI protocol paths;
   отделить controller, protocol, block/partition/fs inspection и mount policy.
3. Свести frame/wire/staging/code buffers и task stacks в HW-MEM profiles с
   отчётом памяти, physical/reserved map и build-time validation. Сначала сохранить
   текущие размеры; изменение бюджетов подтверждать измерениями.
4. Подключить общий ILI9488 core к U-Boot/early adapters, сверить traces и ошибки,
   затем проверить второй аппаратный backend. Настроить BootContext/handoff;
   отключение reset само по себе не означает сохранение состояния панели.
5. Для FreeRTOS завершить аудит task/ISR synchronization, allocator/console/VFS
   и приоритетов; проверить context/VFP/NEON и starvation на CPU0. DMA возвращать
   только после отдельной IRQ/lifecycle интеграции и аппаратных испытаний.

После текущего раздела Hardware data запланирован отдельный этап отзывчивости
Lyra/FreeRTOS: паузы экрана при командах и повторяющиеся ошибки клавиатуры.
Сейчас только [фиксируем наблюдения](tests/lyra-freertos-20261029.md), без изменения
I/O timing, retry и scheduler в рамках архитектурного переноса.

## Остаток по этапам HW

| Этап   | Оставшаяся работа                                                                          |
|--------|--------------------------------------------------------------------------------------------|
| HW-0   | Зафиксированный baseline образов/ревизий/таймингов, cold/warm boot, полная SD и recovery   |
| HW-1   | ADR transport/C boundary/ownership/stages/handoff и аудит закреплённых U-Boot/SPL hooks    |
| HW-MEM | Общие budgets/overrides, единицы в байтах, validation и memory report                      |
| HW-2   | Полная схема routes/voltage/power/revisions; standalone runtime/build и аппаратная приёмка |
| HW-3   | CPU/SoC/controller endpoints, оставшийся native state, lifecycle/recovery и проверка IRQ   |
| HW-4   | U-Boot/early adapters, второй backend, сквозная аппаратная приёмка экрана                  |
| HW-5   | Stage-independent southbridge core, единый FIFO owner, ранние input/power services         |
| HW-6   | BootContext, quiesce/adopt/reset, boot/panic status и проверенный handoff                  |
| HW-7   | Общий SD SPI protocol, SDMMC decomposition, block/partition/fs разделение и приёмка        |
| HW-8   | Codec/PCM/I2S/service, DMA/cache/lifetime и остальные устройства по инвентаризации         |
| HW-9   | Воспроизводимый управляемый SPL, DDR blob, SRAM budget и hardware recovery                 |
| HW-10  | Миграция остальных продуктов, stage manifests, boundary checks и удаление wrappers         |
| HW-11  | Полная regression matrix, ресурсы/скорость, SD/recovery и финальные ограничения            |

## Условия следующих проверок

- Сохранить системную `sd0` read-only и policy съёмной `sd1`; standalone декларация
  пока не является поддержанным способом переключить build.
- Повторить LCD stream/cached на одном кандидате с одинаковыми SPI/форматом/числом
  кадров для PIO и DMA. Отдельно учитывать IRQ deliveries, polling fallback,
  ошибки, SPI wall time и отзывчивость ввода. Отсутствие tearing не заявлять.
- Проверить SD/LCD одновременно, lost IRQ, timeout, channel/manager fault,
  удержание буферов и RAM reload. Host fake MMIO не заменяет выполнение PL330.
- В FreeRTOS проверять PIO и переключение задач отдельно от cooperative DMA;
  наличие собираемого профиля не закрывает KRN-4.
- Повторить сборки Pico и остальных затронутых providers перед выпуском.
  Проверки текущего Lyra-среза не переносить автоматически на всю матрицу.

Аппаратные измерения прежних сборок и последний локальный прогон сохранены в
[release notes](release-0.1.1.md#проверки-и-аппаратные-наблюдения). Новые результаты
добавлять с указанием кандидата и профиля, не возвращая сюда журнал завершённых шагов.
