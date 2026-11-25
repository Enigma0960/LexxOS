# Интеграция FreeRTOS в LexxOS для Luckfox Lyra (RK3506G2)

Дата планирования: 28 октября 2026 года.  
Статус: **исторический план интеграции от 28 октября 2026 года**. FreeRTOS уже запускает shell и устройства; по
состоянию на 04.11.2026 также аппаратно проверены managed MMU, LCD/SD RX PL330 DMA и SD1 FAT RW. Полная многозадачная и
аварийная приёмка ещё открыта. [Текущий статус](boards/picocalc_lyra_m.md#аппаратные-результаты).
Актуальный аудит, расстановка приоритетов и следующий план
развития: [LexxOS FreeRTOS / Kernel audit (31.10.2026)](lexxos-freertos-kernel-audit-20261031.md).

> Чекбоксы ниже отражают первоначальный план и не обновляются автоматически при последующих коммитах. Отслеживать
> прогресс следует по актуальному аудиту и аппаратным журналам.\
> Связанный этап: **KRN-4 Preemptive CPU0** в [общей дорожной карте ARMv7-A](armv7a-irq-dma-kernel-roadmap.md).

## 1. Цель и границы работ

Перевести LexxOS на Lyra с кооперативного планировщика на вытесняющий FreeRTOS для **одного Cortex-A7 (CPU0)**, сохранив
существующие переносимые контракты и полный LexxOS Runtime: shell, ProgramManager, VFS, FatFs, устройства, фоновые
программы и диагностику.

Основные требования:

- FreeRTOS — **сменный scheduler backend**, а не обязательная зависимость кода `app/`, `shell/`, `program/`, `os/` или
  `vfs/`.
- Оставить рабочий `cooperative` backend как fallback и контрольную конфигурацию.
- Сначала запустить FreeRTOS в минимальном bootstrap, **затем** после аудита потокобезопасности подключать полный
  Runtime.
- Сохранить путь загрузки U-Boot/OP-TEE → bare-metal LexxOS, UART0/UART1, SWD/GDB RAM reload и диагностические
  возможности.
- Не сбрасывать чужую конфигурацию GIC, не захватывать Secure/firmware-owned IRQ и не менять правила владения
  DMA-каналами.
- Не ухудшать сохранность данных на `sd0` (системный SDMMC, read-only) и `sd1` (съёмная SPI SD).
- Зафиксировать результаты каждого этапа: сборка, host-тесты, аппаратный прогон, регрессия и возможность отката.

**Вне первой интеграции:** включение MMU/D-cache, защищённые процессы/User mode, запуск вторичных ядер/SMP, новый SPL
или замена U-Boot. FreeRTOS tasks используют одно адресное пространство; они не обеспечивают изоляцию внешних `.lexx`
программ.

## 2. Исходное состояние ветки

| Компонент             | Состояние / точка интеграции                                                                                     |
|-----------------------|------------------------------------------------------------------------------------------------------------------|
| Переносимый scheduler | `src/lexxos/kernel/scheduler.h`: `createTask`, `startScheduler`, `sleepMs`, `yield`, `TaskOptions`, статистика   |
| FreeRTOS в проекте    | `third_party/FreeRTOS` submodule, `config/cmake/freertos_import.cmake`, `config/FreeRTOSConfig.h`                |
| Существующие адаптеры | `kernel/freertos/scheduler_freertos.cpp` (Pico), `kernel/espidf/scheduler_espidf.cpp` (ESP-IDF)                  |
| Lyra scheduler        | `platform/rockchip/rk3506/cooperative_tasks.cpp` и `task_switch.S`; CPU0, добровольное переключение              |
| ARM startup           | `startup.S` и `exception_vectors.S`; ARM state, режимные стеки, VBAR, ранняя инициализация                       |
| Timer/GIC             | `irq_timer.cpp`, `gic_irq_controller.cpp`; Generic Virtual Timer PPI 27, 1 ms, DMA INTID 148/149                 |
| Переносимый IRQ API   | `kernel/irq.h` и `kernel/irq_manager.h`; подписки, ownership, обработчики, тип `Reschedule`                      |
| Memory                | `cpp_runtime.cpp`: собственный C++ heap, без защиты от вытеснения; отдельный кооперативный pool стеков           |
| Устройства            | SPI0 LCD, SPI1 SD, PL330 DMA, приоритетная очередь транзакций; части ожидания и guards рассчитаны на cooperative |
| Полный Runtime        | `app/runtime.cpp` и `app/runtime_tasks.cpp` создают задачи через переносимый scheduler API                       |

Имеющийся RK3506 software budget: C++ heap **256 КиБ**, cooperative task stack pool **96 КиБ**, **8 task slots**; linked
`BOOTRAM` window **8 МиБ** (не вся физическая DDR). Реальные пределы FreeRTOS heap/stack нужно утвердить после анализа
`.map` и измерения stack watermarks.

Это **инвентаризация кода**, а не доказательство работоспособности FreeRTOS на плате.

## 3. Целевая архитектура

```text
LexxOS Runtime
  apps / shell / ProgramManager / VFS / SDK
                |
LexxOS Kernel contracts
  Scheduler / Synchronization / Events / IRQ / Memory
                |
       +--------+---------+
       |                  |
  cooperative          FreeRTOS
  (fallback)        (preemptive CPU0)
       |                  |
       +--------+---------+
                |
RK3506 ARMv7-A platform: startup / exception vectors
GICv2 / ARM Generic Timer / UART / PL330 / MMIO
```

Предлагаемый выбор backend:

```cmake
-DLEXXOS_RK3506_SCHEDULER=cooperative
-DLEXXOS_RK3506_SCHEDULER=freertos
```

До окончания аппаратной приёмки **default = cooperative**. Опции выбора backend должны быть взаимоисключающими; в ELF не
должны одновременно присутствовать два определения `kernel::createTask` и два конфликтующих обработчика IRQ.

Новый код в переносимых слоях не включает напрямую `FreeRTOS.h` или platform-specific headers. FreeRTOS API размещаются
в `kernel/freertos/` и RK3506-specific адаптерах. Существующие `PlatformServices` и `SchedulerBackend` сохраняются.

### Технические решения и точки проверки

1. **Port:** взять официальный `FreeRTOS-Kernel/portable/GCC/ARM_CA9` как **основу для проверки**, а не считать готовым
   портом RK3506. Проверить на Cortex-A7 ABI, ARM/SVC/SYS mode, возврат из исключений, VFP/NEON, совместимость с
   `-mcpu=cortex-a7 -marm -mfloat-abi=soft` и конфигурацию сборки ASM.
2. **Владение IRQ:** при `freertos` использовать один `FreeRTOS_IRQ_Handler` для сохранения/восстановления контекста и
   IAR/EOI. RK3506 dispatcher вызывает существующий `IrqManager` и обработчики устройств **без повторного IAR/EOI**. При
   `cooperative` сохранить проверенный текущий IRQ entry.
3. **Timer:** для FreeRTOS запуск RTOS tick перенести на фазу `vTaskStartScheduler` через требуемые port hooks
   (`configSETUP_TICK_INTERRUPT` / `configCLEAR_TICK_INTERRUPT`). Установить 1000 Hz; раннее uptime/диагностика остаются
   доступны независимо от RTOS tick через аппаратный counter.
4. **GIC security:** Non-secure чтение `GICD_IGROUPRn` может быть RAZ/WI. Нулевой бит не подтверждает Group0; не менять
   security grouping, не глобально обнулять GICD, не трогать firmware-owned IRQ. Реальную доступность PPI 27, INTID 148
   и порядок ACK / device clear / EOI проверять на железе.
5. **GIC priorities:** узнать реализованную разрядность приоритетов и согласовать `configUNIQUE_INTERRUPT_PRIORITIES`,
   `configMAX_API_CALL_INTERRUPT_PRIORITY`, hardware priorities, binary point и ISR-safe вызовы. Нельзя просто перенести
   `0xA0` без проверки ограничений port.
6. **Memory/ABI:** FreeRTOS heap и C++ heap на первом этапе независимы, со своими статистиками и защитой; не смешивать
   `pvPortMalloc/vPortFree` и `new/delete`. Стек API LexxOS задаётся **в байтах**, а глубина стека FreeRTOS — в
   `StackType_t`; преобразование без усечения и с проверкой лимитов.
7. **Diagnostics:** panic/stack overflow/malloc failed должны работать без обычных блокировок и необязательных сервисов;
   сохранить UART/SWD recovery даже при зависании scheduler.
8. **Task semantics:** учесть число task slots, повторное создание задач, корректное освобождение TCB/stack, жизненный
   цикл `TaskSnapshot` и доступ из параллельных задач. `coreAffinity` для первого порта поддерживает только CPU0/any;
   недопустимые значения отклоняются.

## 4. План работ и критерии завершения

Все пункты ниже первоначально открыты. **Компиляция сама по себе не закрывает этап**; для hardware-dependent изменений
нужен отдельный прогон на Lyra.

### Этап 0. Baseline и воспроизводимый rollback

- [ ] Зафиксировать commit стабильного `cooperative` образа, настройки сборки, способ загрузки/отката через SD и
  SWD/GDB.
- [ ] Сохранить снимки GIC/timer/PL330 и базовые замеры UART, LCD SPI0, SD SPI1, DMA; отдельно отметить включённые
  экспериментальные опции.
- [ ] Проверить автономный bootstrap без shell и обычный Runtime в стабильном режиме.

**Готово, когда:** можно восстановить стабильную прошивку и воспроизвести исходную картину ошибок/производительности.

### Этап 1. FreeRTOS build integration

Затрагиваемые файлы:

- `config/cmake/platforms/rk3506_bringup.cmake`;
- `config/cmake/freertos_import.cmake` либо отдельный RK3506 port importer;
- `config/FreeRTOSConfig.h` (только платформенно-ограниченные параметры);
- `src/lexxos/kernel/freertos/` и `src/lexxos/platform/rockchip/rk3506/`.

Задачи:

- [ ] Добавить `LEXXOS_RK3506_SCHEDULER=cooperative|freertos` с validation и сохранением старого default.
- [ ] Подключить фиксированный FreeRTOS submodule/version и официальный GCC ARM_CA9 port (`port.c`, `portASM.S`,
  `portmacro.h`); не копировать и не модифицировать upstream без оформленной причины.
- [ ] Собрать FreeRTOS kernel + memory allocator + hooks, избежать конфликтов символов с cooperative backend.
- [ ] Настроить initial `configUSE_PREEMPTION=1`, tick 1000 Hz, однопроцессорный режим, stack/malloc hooks; проверить
  требования port к FPU и critical sections.
- [ ] Добавить конфигурации минимального bootstrap и production, диагностические сборки обоих scheduler modes.

**Готово, когда:** `cooperative` остаётся неизменным; FreeRTOS-only bootstrap компилируется и линкуется без
неоднозначных обработчиков и duplicate symbols. **Аппаратное переключение пока не заявляется**.

### Этап 2. ARM context switch, GIC и tick

- [ ] Добавить условный путь IRQ/SVC в `exception_vectors.S` с одним владельцем IAR/EOI.
- [ ] Соединить FreeRTOS `vApplicationIRQHandler` и RK3506 IRQ dispatcher/`IrqManager` без изменения semantics других
  interrupt sources.
- [ ] Реализовать настройку и сброс ARM Generic Virtual Timer PPI 27 для FreeRTOS tick; не запускать scheduler-dependent
  tick до инициализации RTOS.
- [ ] Проверить save/restore всех необходимых GPR, CPSR/SPSR, PC, banked stacks, alignment и VFP state согласно реально
  используемому port/ABI.
- [ ] Проверить вложенные IRQ (или обоснованно отключить), critical sections, GIC PMR/priority mask, spurious IRQ и IRQ
  из Secure firmware.
- [ ] Аппаратно запустить две одинаково приоритетные задачи **без `yield()`**, затем CPU-bound low-priority и
  периодическую high-priority задачу.

**Готово, когда:** tick устойчив; preemption/time slicing работают; нет зависаний, повреждений стека и двойного EOI;
UART/SWD recovery сохраняется.

### Этап 3. Scheduler backend и учёт памяти

- [ ] Адаптировать `kernel/freertos/scheduler_freertos.cpp` для RK3506 (либо выделить общий код + platform port): task
  создание/удаление, задержки, уступка CPU, статистика, привязка к ядру.
- [ ] Проверить преобразование stack bytes → FreeRTOS stack depth и корректные минимумы для ARMv7-A.
- [ ] Ввести контролируемый FreeRTOS `heap_4` budget и snapshot общей/свободной памяти, не выдавая FreeRTOS heap за весь
  RAM.
- [ ] Сделать `cpp_runtime.cpp` (`malloc/free/new/delete/realloc`) безопасным для вытесняемых задач; предусмотреть
  ранние вызовы до запуска scheduler и запрет обычных allocations из ISR.
- [ ] Исправить гонки в task registry/snapshots и task teardown, добавить stack watermark, overflow и malloc failure
  diagnostics.
- [ ] Проверить newlib/`errno`/`configUSE_NEWLIB_REENTRANT` и используемые runtime-функции на совместимость с
  многозадачностью.

**Готово, когда:** многократное создание/завершение задач и конкурентные memory operations не повреждают heap/TCB/stack;
диагностические пределы воспроизводимы.

### Этап 4. Kernel synchronization и аудит shared state

- [ ] Добавить переносимые контрактные примитивы mutex (включая priority inheritance), semaphore, event/task
  notification, timeout и ISR-safe wake.
- [ ] Заменить cooperative-only `bool` guard для RK3506 в `BusTransactionQueue`; сохранить FIFO внутри resource priority
  и модель lease/owner.
- [ ] Не допускать активного spinning high-priority waiter, удерживающего CPU у вытесненного владельца; выбрать явный
  sleep/blocking + корректное пробуждение.
- [ ] Пересмотреть `CooperativeEvent` и ожидание DMA: отсутствуют ли lost wakeup, некорректная публикация состояния и
  небезопасные вызовы из IRQ.
- [ ] Проверить global registries, device manager, shared queues, lifecycle, console RX/TX, VFS, ProgramSupervisor и SDK
  resource ownership.
- [ ] Проверить отдельные приоритеты: **GIC IRQ priority**, **FreeRTOS task priority** и **bus transaction priority** —
  разные уровни. Resource queue не прерывает текущую аппаратную транзакцию.

**Готово, когда:** mutex contention, priority inversion, timeout/cancellation и параллельные обращения к одному ресурсу
проходят тесты без deadlock, starvation и races.

### Этап 5. DMA/IRQ-to-task и периферия

- [ ] Реализовать `FromISR`-совместимое пробуждение ожидающей задачи из завершения PL330 DMA, сохранив обязательное
  подтверждение аппаратного события до GIC EOI.
- [ ] Сохранить polling path в качестве диагностического/fallback; не менять SD и LCD DMA protocol одновременно с
  scheduler port.
- [ ] Адаптировать SPI0 LCD, SPI1 SD и I2C transfers к preemption-safe ownership; минимизировать IRQ-disabled участки.
- [ ] Уточнить lifetime DMA buffers, каналы/события PL330 и отмену/тайм-аут операции; не допускать освобождения буфера
  до quiesce DMA.
- [ ] Проверить параллельную работу DMA SD/LCD и отсутствие starvation системных задач.

**Готово, когда:** ожидание DMA блокирует только нужную task; shell остаётся отзывчивым; CRC/readback и fault-recovery
не регрессируют относительно baseline.

### Этап 6. Полный LexxOS Runtime и файловая система

- [ ] Включить `app::Runtime`, `startRuntimeTasks`, shell и ProgramSupervisor с новым backend.
- [ ] Проверить корректную работу console foreground/background, ввода и Ctrl+C; не считать вытесняемую task
  изолированным процессом.
- [ ] Пересмотреть FatFs: сейчас `FF_USE_LFN=1` (static LFN buffer), `FF_FS_REENTRANT=0` и `FF_FS_LOCK=0`. Выбрать
  корректный LFN/reentrancy режим и mutex hooks **или** гарантировать сериализацию через VFS.
- [ ] Проверить VFS mount/unmount/hotplug на конкурентность и сохранение политики `sd0` read-only / `sd1` removable.
- [ ] Проверить LCD console mirror, клавиатуру, системные события, очереди пользовательских программ и приоритетный
  доступ к SPI/I2C/GPIO/UART.

**Готово, когда:** штатная оболочка, пользовательские и фоновые задачи, файловая система и драйверы устойчиво работают
одновременно.

### Этап 7. Диагностика, stress и стабилизация

- [ ] Отображать состояния задач, фактические приоритеты, stack high-water marks, свободный FreeRTOS heap,
  переключения/CPU usage и события overflow.
- [ ] Проверить отсутствие starvation при бесконечной CPU-bound задаче без `yield()`; системная задача и ввод должны
  продолжать работать.
- [ ] Проверить долгий LCD + SD + keyboard + shell stress; измерить IRQ latency (включая p95/p99), CPU busy, FPS и
  verified KiB/s.
- [ ] Проверить cold/warm boot, SD update, SWD RAM reload после зависания, panic-path и восстановление после DMA fault.
- [ ] Выполнить host/firmware regression на других платформах и на обоих RK3506 backends; обновить документацию по
  фактическим результатам.
- [ ] Только после подтверждённой аппаратной приёмки рассмотреть изменение default на `freertos`.

**Готово, когда:** критерии KRN-4 подтверждены протоколами измерений, regression tests и повторяемыми испытаниями на
плате.

## 5. Матрица проверок

| Уровень                  | Проверяем                                                                                                                                        |
|--------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------|
| Static/build             | Два scheduler backend, bootstrap/production/diagnostics; отсутствие duplicate symbols, platform header leaks и regressions для Pico/ESP-IDF/host |
| Host contracts           | Scheduler API semantics, resource queues, ownership, timeout, cancellation, конкурентная регистрация/освобождение задач и событий                |
| GIC/timer hardware       | PPI 27, nesting policy, priorities, spurious handling, один IAR/EOI, отсутствие изменения чужих IRQ                                              |
| Scheduling hardware      | Tick, time slicing, вытеснение CPU-bound задачи, mutex inheritance, stack overflow/malloc failure                                                |
| Аппаратура DMA/устройств | PL330 IRQ→задача, отмена/ошибка, время жизни буфера, совместная работа LCD/SD, совпадение данных PIO и DMA                                       |
| Filesystem/runtime       | FatFs reentrancy, VFS, foreground/background, hotplug, терминал, Ctrl+C, `sd0` остаётся read-only                                                |
| Recovery/performance     | UART/SWD/GDB reload, rollback, CPU load, IRQ p95/p99, LCD FPS, SD verified KiB/s относительно baseline                                           |

У каждого этапа фиксировать: commit SHA, CMake options, версию FreeRTOS, результат host tests, ссылку на UART/GDB log и
аппаратный вердикт. Любой пункт с непроверенной hardware зависимостью остаётся **не закрыт**, даже если успешно собран.

## 6. Основные риски и ограничения

| Риск                                                     | Превентивное действие                                                                      |
|----------------------------------------------------------|--------------------------------------------------------------------------------------------|
| Некорректный Cortex-A7 context save/restore              | Отдельный minimal bootstrap, проверка режимов CPU/ABI/VFP, тест CPU-bound preemption, GDB  |
| Конфликт FreeRTOS IRQ handler и существующего dispatcher | Ровно один IAR/EOI owner для каждого build mode; unit/hardware tests порядка ACK/EOI       |
| Secure vs Non-secure GIC ownership                       | Не выводить Group0 из RAZ/WI, не перезаписывать чужие регистры, fail closed                |
| Гонки allocator и global state                           | Аудит и защита до полного Runtime, стресс-тесты                                            |
| Priority inversion и spinlock deadlock                   | Blocking mutex/notifications, priority inheritance и bounded critical sections             |
| Потеря DMA IRQ либо premature buffer reuse               | ISR-safe notification, completion/timeout/cancel state machine, device ack before EOI      |
| Повреждение FatFs при параллельном I/O                   | Reentrant FatFs или полный per-volume VFS serialization                                    |
| Потеря отладки при зависании                             | Сохранить cooperative firmware и SWD/UART recovery, не изменять загрузчик                  |
| Изменения ABI или upstream FreeRTOS                      | Фиксированный kernel ref/submodule, отдельный RK3506 port adapter и воспроизводимая сборка |

## 7. Первый реализационный инкремент

**Следующий дифф:** только этап 1 и начальная подготовка этапа 2.

1. Добавить CMake selector и FreeRTOS kernel/port sources для RK3506.
2. Подготовить platform-specific FreeRTOSConfig и минимальные hook implementations.
3. Развести обработчики исключений по build mode без преждевременного включения вытеснения для полного Runtime.
4. Реализовать минимальный тестовый путь `LEXXOS_RK3506_SHELL=OFF`.
5. Собрать обе конфигурации и получить аппаратные логи старта. Лишь затем продолжать IRQ/context switching и включение
   preemption для Runtime.

**Не включать одновременно с первым диффом** переписывание SPI/SD/LCD, изменение FatFs, MMU/cache или SMP: так причина
аппаратного сбоя останется диагностируемой.

## 8. Ссылки

- [Архитектура LexxOS](architecture.md).
- [Архитектурная дорожная карта](architecture-roadmap.md).
- [План IRQ/DMA/ядра ARMv7-A, KRN-4](armv7a-irq-dma-kernel-roadmap.md).
- [RK3506 board architecture](boards/picocalc_lyra_m.md#архитектура-порта).
- [Официальный FreeRTOS ARM_CA9 port](https://github.com/FreeRTOS/FreeRTOS-Kernel/tree/main/portable/GCC/ARM_CA9).

Этот документ фиксирует порядок внедрения и критерии готовности. Фактические результаты, найденные ограничения или
изменения решений записываются отдельными PR/коммитами по ходу реализации.

## Дополнение: реализация на 04.11.2026

Пункты `[ ]` и временные этапы выше документируют **план на 28 октября**, а не незавершённый список с сегодняшней датой.
Текущие FreeRTOS CPU0, managed MMU и безопасный NC-bounce PL330 путь проверены при FAT RW нагрузке на `sd1`. Поддержка
записи: CMD24 PIO, CRC16, busy/CMD13 и readback; `sd0` не затрагивается.

Не проверены здесь: длительная конкурентная нагрузка, холодное сравнение файлов на ПК, отказ питания, автоматический
hotplug в FreeRTOS bootstrap и полная изоляция приложений. Основной новый риск — малые high-water остатки стеков
`write`, `append` и `cat`. См. [матрицу](boards/picocalc_lyra_m.md#аппаратные-результаты).
