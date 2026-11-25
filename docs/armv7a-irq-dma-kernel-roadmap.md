# План развития IRQ, DMA и ядра LexxOS на ARMv7-A

Обновлено 29 октября 2026. Статус: **KRN-1…KRN-4 частично реализованы; аппаратная приёмка этапов открыта**.
Выполненные срезы LCD/DMAC0 и базовый FreeRTOS — в
[черновике 0.1.1](release-0.1.1.md). Ниже остаются целевые контракты и критерии приёмки.
Целевая первая платформа: RK3506G2, Luckfox Lyra M / PicoCalc, CPU0.
Переносимые контракты должны допускать другие SoC и host/fake backends.

Связанные материалы: [активный roadmap](../roadmap.md),
[план аппаратной и загрузочной архитектуры](hardware-boot-architecture-plan.md),
[архитектурная дорожная карта](architecture-roadmap.md),
[ревизия RK3506](boards/picocalc_lyra_m.md#архитектура-порта) и
[экспериментальный PL330 SD](boards/picocalc_lyra_m.md#история-внедрения-и-эксперименты).

## 1. Цель, границы и отношения с HW-0…HW-11

Развить существующий bare-metal ARMv7-A runtime в систему с управляемыми
прерываниями, неблокирующими DMA-транзакциями и вытесняющими задачами.
Первый прикладной результат — ускорение LCD по SPI0 и SD PicoCalc по SPI1,
освобождение CPU во время I/O и сохранение отзывчивости shell/программ.

Это **отдельная программа KRN-***. Она не добавляет скрытых обязательных работ
в HW-0…HW-11 и не мешает окончанию текущей миграции драйверов, плат и U-Boot.
В HW-1, HW-MEM, HW-2/HW-3, HW-6/HW-7 следует закрепить контракты ресурсов,
памяти и жизненного цикла, необходимые будущему kernel. Полноценные FreeRTOS
port, MMU, User mode и SMP выполняются и принимаются отдельно.

Не входит в первые этапы: собственный SPL, замена DDR blob, RTOS в U-Boot,
Linux/POSIX-совместимость, доступ внешних программ к raw MMIO/GIC/PL330,
снятие защиты системной SD и обязательный запуск нескольких CPU.

## 2. Границы на дату планирования

Таблица фиксирует срез 29.10.2026. Последующая реализация FreeRTOS,
управляемой MMU и DMA через NC-буферы описана в
[руководстве Lyra](boards/picocalc_lyra_m.md); старое ограничение DMA
при MMU/D-cache не относится к рабочему профилю `MMU_MANAGED + DMA_SAFE`.

| Подсистема                 | Текущая граница                                                         | Остаток работ                                                                           |
|----------------------------|-------------------------------------------------------------------------|-----------------------------------------------------------------------------------------|
| ARM exceptions / scheduler | Cooperative и экспериментальный FreeRTOS ARM_CA9 adapter на CPU0        | Проверка context/VFP/NEON, preemption и starvation на устройстве                        |
| GIC и timer                | IRQ framework и экспериментальные adapters; отдельные режимы интеграции | Ownership/priority/quiescence audit, согласование с RTOS IRQ entry и аппаратная приёмка |
| DMA LCD                    | Instance-owned controller, direct wire buffer, два кадра, dirty regions | Общий lifecycle/recovery, замеры актуальной версии и совместный stress                  |
| DMA SD                     | PL330 channels/events 0/1, общий DMAC0 service                          | IRQ/full-duplex qualification и перенос native state                                    |
| Memory                     | Статические DDR workspaces, DMA только при выключенных MMU/D-cache      | Общие profiles, physical/reserved map, DMA/cache policy                                 |
| SPI leases                 | Общие очереди и critical sections для FreeRTOS metadata                 | Полный lock/priority inversion audit и аппаратная нагрузка                              |

Это историческая исходная граница: тогда FreeRTOS использовал PIO.
В текущем managed-MMU + `DMA_SAFE` профиле PL330 работает для LCD TX
и SD RX через Normal-NC staging; SD1 CMD24 **запись** остаётся SPI1 PIO.
См. [актуальную сводку](boards/picocalc_lyra_m.md#аппаратные-результаты).
Собираемый профиль не подтверждает аппаратную готовность KRN-4.

Точки входа:

- [irq_timer.cpp](../src/lexxos/platform/rockchip/rk3506/irq_timer.cpp),
  [exception_vectors.S](../src/lexxos/platform/rockchip/rk3506/exception_vectors.S),
  [startup.S](../src/lexxos/platform/rockchip/rk3506/startup.S).
- [cooperative_tasks.cpp](../src/lexxos/platform/rockchip/rk3506/cooperative_tasks.cpp),
  [task_switch.S](../src/lexxos/platform/rockchip/rk3506/task_switch.S),
  [scheduler.h](../src/lexxos/kernel/scheduler.h).
- [picocalc_lcd_dma_pl330.cpp](../src/lexxos/devices/rk3506/picocalc_lcd_dma_pl330.cpp),
  [picosd_dma_pl330.cpp](../src/lexxos/devices/rk3506/picosd_dma_pl330.cpp),
  [picosd_spi1_hw.cpp](../src/lexxos/devices/rk3506/picosd_spi1_hw.cpp).
- [bus_transaction_queue.cpp](../src/lexxos/devices/bus/bus_transaction_queue.cpp),
  [cpp_runtime.cpp](../src/lexxos/platform/rockchip/rk3506/cpp_runtime.cpp),
  [FreeRTOS scheduler backend](../src/lexxos/kernel/freertos/scheduler_freertos.cpp).

Важное разграничение носителей: системная SD `sd0` подключена через SDMMC,
съёмная SD PicoCalc `sd1` — через SPI1. Миграция `sd1` на DMA не меняет
`sd0`, её RO/recovery policy, разделы и правила монтирования.

## 3. Целевые архитектурные уровни

1. **Архитектура ARMv7-A:** вход/выход исключений, контекст ABI, CPSR/SPSR,
   состояние VFP/NEON, барьеры памяти, примитивы кеша/TLB/MMU, регистры таймера.
   Здесь нет адресов RK3506 и board wiring.
2. **Реализация SoC RK3506:** MMIO, маршрутизация и защита GIC, PL330, привязка запросов,
   clocks/reset/GRF, SPI, допустимые каналы, interrupt IDs, physical ranges.
3. **Сервисы ядра:** менеджеры IRQ/DMA, планировщик, события,
   семафоры/mutex, allocator, resource/lease ownership и диагностика.
4. **Device transports:** SPI endpoints, LCD ILI9488, SD SPI/SDMMC; драйверы
   не сбрасывают глобально GIC/DMAC0 и не выбирают board product.
5. **Composition:** выбранный продукт связывает controllers, IRQ/requests,
   CS, memory regions и policy без скрытого singleton-ресурса.
6. **SDK:** программы получают зарегистрированные устройства через capability
   и lease/priority queue; не получают сырые PL330 channels или GIC IRQ.

Семантические примеры новых контрактов (названия и C++ ABI пока не утверждены):

- `requestIrq(source, owner, priority, handler) -> handle/error`;
  `releaseIrq(handle)`, маскирование/размаскирование, статистика по источникам.
- `acquireDmaChannel(endpoint, owner)`;
  `submit(handle, buffer, direction, deadline) -> transfer/error`;
  `wait/cancel(transfer)` с гарантированным lifetime буфера.
- `DmaBuffer`: CPU address, физический/bus address, размер, alignment,
  направление, memory attributes и состояние cache ownership.
- ISR-to-task notification/событие: до планировщика — bounded polling,
  после — блокирующее ожидание задачи без busy-wait.

Новые контракты должны быть небольшими, проверяемыми и не дублировать
существующие interfaces ради одного временного backend.

## 4. Подсистема IRQ

**Можно реализовать уже сейчас:** startup и рабочие IRQ таймера/DMAC0 есть.
Не требуется ждать своего SPL, MMU или FreeRTOS.

Первый программный срез — [переносимый IRQ Manager](irq-manager-implementation.md):
фиксированный пул подписок, `IrqController` boundary, shared IRQ и host tests.
Интеграция зависит от build flags; наличие manager и adapters само по себе
не закрывает KRN-1. FreeRTOS IRQ entry требует отдельного согласования.

- Разделить механизмы ARM exception, RK3506 GIC и общий `IrqManager`.
  Реестр обработчиков хранит source, owner, priority, trigger policy и status.
- При регистрации проверять ownership, Group1/non-secure, target CPU,
  enabled/pending/trigger и конфликт с текущим владельцем. Не обнулять GICD,
  не переназначать secure Group0/OP-TEE IRQ.
- В ISR: подтверждать источник на периферии в верном порядке, обновлять
  компактный atomic status, будить задачу при поддержке scheduler, делать EOI.
  Никаких обычных malloc, VFS, printk или блокирующих mutex из ISR.
- Обрабатывать spurious/unexpected IRQ диагностикой с безопасной политикой
  masking/fault; предотвращать endless level-triggered interrupt storm.
- Сохранить PPI 27 и DMAC0 IRQ148/149 как контрольные consumers. Новые UART/GPIO/SPI
  sources добавлять после проверки RK3506 TRM и физического теста.
- Предусмотреть будущие per-CPU handlers/affinity, но до SMP оставить CPU0.

Не смешивать три приоритета: hardware GIC, scheduler task и очередь
транзакций SPI. Приоритет IRQ сам по себе не гарантирует доступ к шине.

### Первый срез KRN-1: переносимый контракт (без смены backend)

- `kernel/irq.h` определяет `IrqDomainId` и `IrqSourceId`: source задаётся
  парой domain/line; GPIO bank может быть каскадным доменом с parent IRQ.
- `IrqVectorModel` описывает Fixed/Relocatable/SdkManaged отдельно от
  `runtimeSubscriptions`: фиксированная таблица допускает заранее связанный
  trampoline и динамическую подписку, если backend это реализует.
- `IrqService` — внедряемый контракт с `domainAt`, `subscribe`, `setEnabled`,
  `unsubscribe`. Регистрация из task/init context, по умолчанию disabled;
  ISR callback не блокирует и сам подтверждает свой peripheral source.
- Shared source разрешён только с явным согласием backend и всех владельцев:
  все подписчики должны проверять собственный статус, dispatcher не может
  завершить обход после первого `Handled`. Конфликт trigger/priority — ошибка.
- `unsubscribe` обязан гарантировать quiescence callbacks до освобождения
  userData; устаревшие handles и чужие owners не могут удалить подписку.
- Вендорные reserved/secure IRQ остаются за их владельцами. Cortex-M
  SysTick/PendSV/SVC, ARM GIC secure IRQ и ESP-IDF RTOS interrupts не
  перехватываются без согласованного handoff и полномочий.
- Этот срез не подключает новый интерфейс к `PlatformServices`, не меняет
  действующий GPIO IRQ API и не объявляет KRN-1 аппаратно завершённым.

## 5. Менеджер DMA / PL330

**Можно реализовать до вытесняющего scheduler**, используя bounded polling
и экспериментальный opt-in. Оба SPI пользуются одним DMAC0.

- Один менеджер владеет каналами, PL330 events, DMA requests, GRF routes,
  clocks и IRQ completion. LCD SPI0 использует channel 2/request 0,
  SD SPI1 — channels 0/1/requests 2/3. Не сбрасывать DMAC0 при отказе SPI0,
  если SPI1 ещё работает.
- `submit` запускает транзакцию, но не ожидает её завершения. `wait` не
  удерживает CPU busy-loop после появления scheduler. IRQ completion
  уведомляет waiter; перед снятием LCD CS/DC отдельно проверяется SPI drain.
- Длительность владения source/destination buffer покрывает всю DMA transfer.
  `cancel`, timeout, unmount, reboot, debug RAM reload и handoff обязаны
  останавливать свои каналы с дедлайном до освобождения памяти.
- При PL330 fault фиксировать manager/channel registers **до** reset/kill;
  прекращать только принадлежащие channels, выключать соответствующие DMA
  requests и переводить endpoint в явное failed/reprobe state.
  Нет PIO fallback посреди частично выполненного SD/LCD пакета.
- Начать с проверенной модели **MMU OFF / D-cache OFF**. В memory API сразу
  предусмотреть DMA/bus address, выравнивание, direction, cache ownership
  и поздние clean/invalidate операции; CPU pointer не равен автоматически
  физическому адресу после включения MMU.
- Раздельно измерять setup, microprogram generation, copy, transfer,
  hardware drain, IRQ latency, ошибки, direct/bounce usage и восстановление.

### LCD SPI0 / ILI9488

Текущий RGB666 wire format: полный кадр 320×320×3 = 307 200 байт.
Идеальный предел при 46,875 МГц — около 19,1 FPS без protocol overhead;
DMA не может превысить физическую частоту SPI.

1. Сохранить PIO-эталон и сравнение цветов/ориентации.
2. Провести на железе fault/IRQ квалификацию DMA TX.
3. Проверить direct source, staging и удержание буферов на актуальной версии;
   измерить вклад конвертации, ожидания и переключения задач.
4. Проверить два кадра, partial updates и backpressure под совместной нагрузкой;
   измерить FPS, CPU busy, SPI occupancy и responsiveness. 16-битный wire format
   проверить отдельно на панели, не предполагать поддержку по RGB565 API.

### Съёмная SD PicoCalc / SPI1

SPI 6/12 МГц даёт идеальный поток 0,75/1,5 МБ/с до token/CRC/command overhead.
Основной эффект DMA — освобождение CPU и приближение к этому пределу.

1. Сохранить CRC16, CMD17/CMD18/CMD24, PIO/FIFO и readback эталоны.
2. Проверить full-duplex TX dummy clocks + RX, оба канала и оба completion
   event; исключить overflow, timeout и некорректный CS/CMD12.
3. Добавить direct DMA-safe RX и bounce mode с одинаковым результатом.
4. Оптимизировать CMD18 и read-ahead на BlockDevice/FAT уровне, не
   перемещая mount policy в transport.
5. Дробить долгие обращения только в протокольно допустимых точках.
   Bus priority выбирает следующую транзакцию, но не прерывает текущую.
6. SDMMC/boot `sd0` исследовать отдельно, без неявного доступа на запись.

## 6. Вытесняющая многозадачность

Предпочтительный первый backend — **FreeRTOS для ARMv7-A на CPU0**.
Базовая интеграция ARM_CA9 port уже добавлена и собирается. До приёмки остаётся
проверка Cortex-A7/RK3506, GIC, Generic Timer, ABI, VFP/NEON и поведения под нагрузкой.

Оставить общий §kernel::createTask/startScheduler/sleepMs/yield§ фасад.
Сохранить два build mode: `cooperative` (fallback) и `freertos`.

Для завершения экспериментального порта и его приёмки:

- Расширить context save/restore для IRQ-driven switch, PC/CPSR/SPSR,
  банковых стеков и используемого floating-point состояния.
- Интегрировать RTOS timer tick, IRQ-to-task wake, nested IRQ policy
  и критические секции через согласованный GIC/ARM port.
- Ввести ready/blocked состояния, task priorities, семафоры, mutex
  с priority inheritance, event notifications и timeout/cancellation.
- Пересмотреть cooperative-only `BusTransactionQueue`: RK3506 bool guard
  не является настоящей preemption-safe блокировкой. Убрать потенциальный
  spinning высокоприоритетных задач над preempted lock owner.
- Проверить allocator `cpp_runtime.cpp`, global registries, console,
  VFS/device owners, очереди SDK и обработку task teardown.
- Выполнить тест с CPU-bound задачей без `yield`: более приоритетные
  system tasks, ввод и DMA completion продолжают выполняться.

Preemptive threads **не являются изолированными процессами**: внешние
`.lexx` программы останутся в одном адресном пространстве до MMU/User mode.

## 7. MMU, D-cache, защита процессов, SMP

MMU и D-cache не требуются для первого рабочего DMA. Позднее необходимо:

- Утвердить DDR map, OP-TEE/bootloader reservations, memory pools и
  карту размещения runtime/ELF, прежде чем включать MMU.
- Настроить normal/device memory attributes, translation tables, TLB
  management и гарантировать независимый panic/debug UART.
- Для DMA применять bus/physical addresses, cache-line alignment,
  clean перед TX, invalidate/sync после RX и память, доступную DMA.
  Аппаратная приёмка обязательна при **D-cache ON**.
- В отдельном этапе создать User mode, SVC/system calls, per-process
  permissions, page tables и fault isolation для внешних программ.
- Только затем исследовать SMP: запуск secondary CPU, coherency,
  SGI/IPI, per-CPU stacks, interrupt affinity и locks/scheduler migrations.

## 8. Этапы с приёмкой и зависимости

| Этап                         | Зависимости                        | Результат и критерий завершения                                                                                                                                      |
|------------------------------|------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **KRN-0 Baseline**           | HW-0                               | Сохранены working image/recovery, GIC/PL330/DDR snapshot, PIO LCD/SD timings, CRC/hash, CPU load и UART/SWD сценарии                                                 |
| **KRN-1 IRQ framework**      | HW-1, HW-3                         | Работают owner/priority/register/unregister, PPI 27 и DMAC0 IRQ148/149, fault/spurious диагностика; нет IRQ storm и чужих GIC writes                                 |
| **KRN-2 DMA framework**      | KRN-1, HW-MEM, HW-3, HW-6          | PL330 channel/route ownership, submit/wait/cancel, buffer lifetime, fault/recovery; одновременный SPI0+SPI1 stress                                                   |
| **KRN-3 Async LCD/SD**       | KRN-2, HW-4, HW-7                  | LCD direct/double buffer/dirty regions, SD DMA+CMD18/read-ahead; PIO data parity и измеренный CPU/FPS/KiB/s результат                                                |
| **KRN-4 Вытеснение на CPU0** | KRN-1, аудит блокировок/аллокатора | Порт FreeRTOS ARMv7-A, пробуждение из ISR, приоритеты, мьютекс с наследованием приоритета, кооперативный резервный режим, тест голодания при вычислительной нагрузке |
| **KRN-5 MMU/D-cache**        | KRN-2, KRN-4, memory map           | Корректная DDR mapping и device attributes; LCD/SD DMA с включённым D-cache и воспроизводимый panic/debug path                                                       |
| **KRN-6 User isolation**     | KRN-5                              | User mode/SVC/permissions; fault пользовательской программы не повреждает kernel                                                                                     |
| **KRN-7 SMP**                | KRN-4, KRN-5                       | Secondary CPU bring-up, IPI/affinity/coherency, parallel stress с DMA/IRQ без race                                                                                   |

Первый практический критический путь: **KRN-0 → KRN-1 → KRN-2 → KRN-3**.
KRN-4 допустимо прототипировать параллельно, но включать только после
аудита потокобезопасности. KRN-5/6/7 не блокируют полезный DMA на CPU0.

Зависимости HW-* обозначают готовность необходимых контрактов/границ,
а не требование предварительно завершить HW-11. Каждый KRN закрывается
после отдельно зафиксированного hardware pass, а не только компиляции.

## 9. Матрица проверок

- **Компьютер/имитация:** владение IRQ, защищённый/чужой источник, порядок ACK/EOI,
  ложные IRQ, устаревшее завершение DMA, отмена передачи, время жизни буфера,
  priority queue fairness и deadlock/timeout fault injection.
- **Build:** bootstrap и production/diagnostics RK3506; оба scheduler
  mode; Pico, ESP-IDF, STM32 и host без утечки RK3506 headers в core.
- **IRQ hardware:** непрерывность системного времени, latency,
  маскирование ошибочного GIC, IRQ под нагрузкой, восстановление через UART/GDB.
- **DMA hardware:** реальные bytes/CRC/hash PIO=DМа (case sensitive
  диагностический вывод проверять отдельно), PL330 faults, SPI overflow,
  извлечение SD, совместная работа LCD/SD, перезапуск и повторное определение.
- **Scheduler:** бесконечная CPU-bound задача, foreground/background,
  mutex contention, priority inversion, task stacks, Ctrl+C, длительный I/O.
- **Жизненный цикл:** холодная/тёплая загрузка, U-Boot → LexxOS, перезагрузка RAM через SWD, исключения,
  bounded DMA quiesce, сохранение secure resources, recovery SD image.
- **Производительность:** FPS полного/частичного LCD, переданные байты, подготовка/передача DMA/
  drain, SD verified KiB/s, CPU busy %, p95/p99 IRQ latency и сравнение
  с KRN-0. Отделять теоретические значения, тестовую гипотезу и замеры.

Предлагаемые будущие команды: `irq list/stats`, `dma list/stats/test`.
Существующие `display-test` и `sd-hw-bench` не удалять до parity.

## 10. Ключевые риски

| Риск                                 | Защитная мера                                                |
|--------------------------------------|--------------------------------------------------------------|
| Чужой secure IRQ/OP-TEE resource     | Не сбрасывать GIC глобально, Group1/ownership validation     |
| Конфликт PL330 channel/event         | Единый allocator и channel-scoped kill, без global reset     |
| Повреждение DMA-буферов из-за кеша   | Память для DMA, физическое отображение, синхронизация кеша   |
| Неполная SD транзакция               | Fail/reprobe, CRC/readback, никакого mid-packet PIO fallback |
| Preemption ломает старые locks       | Аудит allocator/queues до scheduler switch, ISR-safe API     |
| U-Boot/DDR handoff меняет IRQ/pinmux | Контракт HW-6, bounded quiesce, recovery image               |
| Ускорение в ущерб интерактивности    | CPU busy, input/IRQ latency и starvation вместе с FPS        |

Отдельные ADR до реализации: политика IRQ/security ownership, модель DMA
buffer и cacheability, общие события ISR→task, выбор конкретного FreeRTOS
ARMv7-A port и контракт передачи ресурсов boot/runtime.

**Приоритет:** надёжный IRQ и DMA на CPU0 → вытесняющие задачи →
MMU/D-cache и изоляция → SMP. Статусы KRN-* ведутся в едином корневом
`roadmap.md`, а этот документ хранит подробный технический план.

## Дополнение: аппаратный срез 04.11.2026

- FreeRTOS CPU0 и managed MMU/D-cache работают совместно с двумя PL330 DMA потребителями.
- LCD TX: 119 chunks, 5280 КиБ/с; SD RX: 257 секторов, 965 КиБ/с; аппаратных ошибок/тайм-аутов DMA не зафиксировано.
- SD1 FAT RW: аппаратный тест с двумя opt-in флагами подтвердил `write`, `append`, `cp`, `sync`, `unmount` и RO remount.
  Следующее изменение вернуло board-default RW при boot и ручном remount **без opt-in**; перепроверка нового пути
  ожидается. `sd0` — системная SDMMC, запись из PicoCalc storage service не разрешена.
- Остаются KRN-цели по конкурентным стресс-тестам, аварийному восстановлению, универсальному hotplug, корректности стека
  файловых команд, CPU1/CPU2 и SMP.

Реализация части KRN-инкрементов не означает приёмки всей архитектурной дорожной карты. Полный список подтверждённого и
открытого: [сводка](boards/picocalc_lyra_m.md#аппаратные-результаты).
