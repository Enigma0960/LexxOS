# LexxOS: аудит FreeRTOS и план развития Kernel/Scheduler

> **Состояние аудита на 31.10.2026 — историческая база.** С 04.11.2026
> порт Lyra получил рабочие managed
> MMU/D-cache, NC-bounce LCD/SD RX DMA и FAT RW для `sd1`. После базового
> аппаратного теста запись стала board-default (без opt-in); повторный smoke
> нового boot/remount пути ожидается.
> Старые таблицы ниже сохраняют исходную дату; см.
> [новый аппаратный статус](boards/picocalc_lyra_m.md#аппаратные-результаты).
> Дата аудита: 31 октября 2026 года.\
> Статус: архитектурный аудит; не является подтверждением аппаратной приёмки последнего образа.

## 1. Цель

После успешного запуска LexxOS на Luckfox Lyra (RK3506G2 / Cortex-A7, CPU0) с вытесняющим FreeRTOS перейти от «RTOS
запускает Shell и драйверы» к полноценному многозадачному Runtime. Ключевое требование: контракты планировщика,
синхронизации, событий, IRQ, памяти и жизненного цикла программ должны быть платформенно-независимыми. FreeRTOS — один
из backend, а не зависимость `app/`, `shell/`, `program/`, `vfs/`.

Не допустить регрессий на Pico/RP2040, Pico 2/RP2350, ESP-IDF, STM32F4, host/sim. Оставить cooperative backend для
RK3506 как диагностический fallback.

## 2. Фактическое состояние

| Подсистема                       | Подтверждено исходным кодом                                                     | Открытый вопрос                                                             |
|----------------------------------|---------------------------------------------------------------------------------|-----------------------------------------------------------------------------|
| RK3506 startup / UART / FreeRTOS | ARM_CA9-derived port, IRQ entry, Generic Virtual Timer PPI27, scheduler backend | Полная валидация IRQ priorities / nested IRQ, переносимость                 |
| Tasks                            | `createTask`, `sleepMs`, `yield`, snapshots, stack high-water mark              | Безопасное завершение/повторное использование task slots, лимит 8           |
| Memory                           | FreeRTOS `heap_4` 128 KiB; независимый C++ heap 256 KiB                         | MMU-off atomics, allocator/ABI, новые профили памяти                        |
| Shell / UART                     | Системный shell task, сериализация UART task writes                             | Интерактивный prompt при асинхронных сообщениях                             |
| LCD                              | Double-buffered RGB565 presenter; FreeRTOS path uses PIO                        | FreeRTOS DMA, длительный стресс нового renderer                             |
| SPI SD / FatFs                   | SD1 FAT только для чтения, монтирование при загрузке, рекурсивный мьютекс тома  | статический буфер LFN FatFs, гонки монтирования, горячее подключение/RW     |
| PicoCalc keyboard / I2C          | Периодический poll, диагностика и recovery                                      | Длительная стабильность под нагрузкой                                       |
| System events                    | Переносимая служба существует                                                   | В RK3506 FreeRTOS активирован `CONSOLE_ONLY`, часть фоновых задач отключена |
| GIC / DMA                        | IRQ delivery, IRQ148, PL330 drivers и cooperative DMA path                      | Общий IRQ ownership, FromISR wake, buffer lifetime/cancel                   |

Наблюдения: `docs/tests/lyra-freertos-20261029.md`, `docs/tests/lyra-freertos-20261030-fixes.md`. Документирован быстрый
benchmark без failures в одном из прогонов 30 октября; после этого изменена модель вывода LCD. Последний commit не
эквивалентен новому подтверждённому soak-тесту.

## 3. Архитектурные решения

```text
apps / shell / program / vfs / devices
                |
       LexxOS kernel contracts
       Scheduler / Sync / Event / IRQ / Memory
                |
     +----------+----------+------------------+
     |          |          |                  |
 FreeRTOS    Cooperative  Host/Simulator  ESP-IDF
     |
   platform / chip / board
 (RK3506, RP2040, RP2350, STM32...)
```

- Общий C++ API не включает `FreeRTOS.h`, platform registers или `esp_*.`
- Scheduler отвечает за задачи, их создание/завершение, состояния, приоритеты, snapshots, delays; ProgramSupervisor
  отвечает за прикладные процессы и их ресурсы.
- Ошибки параметров (priority, affinity, stack, capacity) возвращаются без побочных эффектов.
- `TaskOptions.stackSizeBytes` всегда в байтах; преобразование в глубину стека выполняет backend с проверкой
  переполнения и округлением вверх. Особенности ESP-IDF (размер стека в байтах) должны учитываться отдельно.
- Разделять IRQ priority (GIC/NVIC), FreeRTOS task priority и приоритет очереди ресурсов: очередь не вытесняет текущую
  SPI/I2C транзакцию.
- Тайм-аут измерять по монотонным часам, а не по числу вызовов `sleepMs(1)`.
- Lifetime task slot и пользовательского контекста не заканчивается до того, как завершившаяся задача гарантированно
  перестала обращаться к ним.
- Отмена DMA/SD/LCD требует quiesce аппаратного устройства до освобождения памяти.
- ISR не должен ждать mutex, выделять память или обращаться к блокирующим средствам вывода.

## 4. Приоритет P0: корректность ядра

### KRN-S1. Task lifecycle и валидация

Файлы: `kernel/scheduler.h`, `kernel/freertos/scheduler_freertos.cpp`, `kernel/espidf/scheduler_espidf.cpp`,
`program/program_supervisor.cpp`.

- Проверять priority, affinity, stack bounds и наличие слота *до* запуска задачи.
- Исключить переиспользование `FreeRtosTask` до завершения кода trampoline и безопасного удаления задачи FreeRTOS.
- Исключить повторное использование `ProgramProcessSlot` до завершения его task и освобождения контекста.
- Развести logical finished, task suspended/retired и reclaimed; обеспечить корректные snapshots/watermarks.
- Определить политику `taskCount`: активные задачи, в отличие от отложенных к освобождению TCB.

Приёмка: массовое создание/завершение задач, принудительное вытеснение непосредственно перед task teardown, повторное
использование слотов без повреждения памяти и use-after-free.

### KRN-S2. Синхронизация

Файлы: `kernel/` (новые переносимые контракты), `kernel/freertos/` (backend), `devices/bus/bus_transaction_queue.cpp`.

- Mutex с priority inheritance, recursive mutex, semaphore, event/notification, ISR-safe wake, bounded timeout.
- Синхронизация должна работать при FreeRTOS на RK3506/Pico и в ESP-IDF, с реализациями/fallback для host и cooperative
  backend; не делать заглушки «успешная блокировка» для настоящей конкурентной конфигурации.
- Заменить периодическое `wait(1)` на ожидание события; сохранять FIFO внутри bus priority и fairness.
- Не удерживать short critical section на время I/O, callbacks, dynamic allocation или `yield`.

Приёмка: многократные contention/cancellation/timeout, отсутствие lost wakeups, deadlock, starvation и priority
inversion.

### KRN-S3. Память и атомарные операции

Файлы: `platform/rockchip/rk3506/cpp_runtime.cpp`, `config/FreeRTOSConfig.h`, `config/memory/`.

- Проверить `std::atomic` RMW, LL/SC, C++ ABI/newlib на RK3506 без MMU; известен Data Abort на `LDREXB`.
- Раздельно показывать RTOS heap, C++ heap, task stacks, DMA memory и watermarks.
- В перспективе — минимальный identity-mapped MMU с корректными атрибутами Normal RAM; полноценная память процессов —
  отдельный этап.

### KRN-S4. Конкурентный доступ VFS/FatFs

Файлы: `fs/fat/fat_volume.cpp`, `third_party/fatfs/source/ffconf.h`, `vfs/`.

- Per-volume mutex уже есть, но `FF_USE_LFN=1` использует static LFN work buffer; `FF_FS_REENTRANT=0`, `FF_FS_LOCK=0`.
- До включения параллельного доступа гарантировать глобальную сериализацию FatFs, либо перейти на reentrant-режим с
  корректным LFN и системными mutex hooks.
- Защитить mount/unmount/handle lifetime и реакции на исчезновение носителя; сохранить SD0 RO и SD1 RO до отдельной
  проверки записи.

## 5. Приоритет P1: полноценный Runtime и I/O

### KRN-R1. Убрать постоянный console-only профиль

Файлы: `config/cmake/platforms/rk3506_bringup.cmake`, `app/runtime_tasks.cpp`.

- Сейчас FreeRTOS+shell выставляет `LEXXOS_RK3506_FREERTOS_CONSOLE_ONLY`: из штатного набора исключены system events и
  некоторые board runtime services.
- Разделить профили `bootstrap`, `minimal`, `full`, `diagnostic`. Включать system-event/board tasks после аудита их
  shared state.
- Прогон foreground/background, Ctrl+C, autostart и нескольких приложений.

### KRN-I1. Унифицированный IRQ Manager

- Выделить одну точку GIC IAR/EOI; `vApplicationIRQHandler` вызывает переносимый dispatcher и не подтверждает IRQ второй
  раз.
- Согласовать GIC priority mask с `configMAX_API_CALL_INTERRUPT_PRIORITY`, принадлежность firmware-owned IRQ, `FromISR`
  API и non-secure режим.
- Сейчас `LEXXOS_RK3506_MANAGED_TIMER_IRQ` несовместим с FreeRTOS по проверке CMake; убрать ограничение только после
  доказанной интеграции.

### KRN-I2. Асинхронное PL330 DMA

- Task запускает SD/LCD DMA, блокируется на notification, IRQ148 подтверждает hardware event и будит нужную задачу через
  `FromISR`.
- Верифицировать deadline, partial completion, late IRQ, отмену, lifetime буферов, общие каналы/события; polling
  fallback оставить.
- Сейчас LCD FreeRTOS profile явно использует `SPI0 PIO; DMA disabled`; сравнивать FPS и latency после аппаратного
  возвращения DMA.

### KRN-R2. Пользовательские приложения

- Поддержать cooperative cancellation и остановку блокирующих I/O, учёт ресурсов приложения, process IDs с generation и
  безопасный teardown.
- Не приравнивать FreeRTOS task к защищённому процессу: пока нет изоляции адресных пространств.

## 6. Приоритет P2/P3

- P2: event-driven input/console, hotplug SD, безопасная RW-политика, корректный system shutdown/reboot, долгие
  soak/stress.
- P2: расширить `tasks`, `top`, `mem`, `irq-list`, `dma-info`, `bus-info` — состояния, high-water marks, CPU usage,
  latency, отказоустойчивость.
- P3: tickless idle, MMU memory protection, user mode, SMP после приёмки CPU0. Не заменять U-Boot ради FreeRTOS.

## 7. Рекомендуемая последовательность и критерии приёмки

1. **Kernel correctness:** task lifecycle + параметры, синхронизация, память, FatFs.
2. **Full Runtime:** system events, board services, приложения и ресурсные контракты.
3. **Async I/O:** IRQ ownership, SD/LCD DMA, notification, устранение polling.
4. **Regression / portability:** два backend RK3506, RP2040/RP2350, ESP-IDF, STM32 и host.
5. **Hardware soak:** shell + keyboard + SD read + LCD output + 2–4 программы одновременно; сравнить показатели с
   baseline, проверить отсутствие зависаний, stack overflow, unexpected IRQ и роста задержек.

Каждая задача требует: тест контрактов на host, успешной сборки затронутых платформ, отдельного аппаратного прогона на
Lyra и фиксации результатов. Успешная сборка или unit test не означает hardware PASS.

## 8. Первый инкремент

Первым изменением выполнить **KRN-S1**: переносимая валидация TaskOptions и безопасное освобождение FreeRTOS task slots,
сохранив API совместимым. Затем **KRN-S2**: проектирование и тестирование Sync API с FreeRTOS backend; только после
этого менять bus queue / FatFs / DMA.

## 9. Реализация первого инкремента (31 октября 2026)

Коммит `71f0bb7765d950fb8a95c356bdd816e1b99c61c1`: добавлена платформенно-независимая политика проверки
приоритета/affinity и преобразования размера стека в `kernel/task_policy.h`. Общий FreeRTOS backend (Lyra/Pico) больше
не публикует слот свободным до self-suspend завершившейся задачи; удаление выполняется из другой задачи после
подтверждения `eSuspended`. ESP-IDF получил проверку priority/affinity, но полная унификация его task lifecycle и
SMP-safe registry остаются открытыми. Добавлены проверки гонки retirement/reuse в `freertos_scheduler_tests.cpp`.

**Не завершено на дату аудита 31 октября:** mutex/semaphore/event abstraction, перевод BusTransactionQueue на blocking
wait, синхронизация FatFs/VFS, SMP-safe registry и аппаратная проверка этого изменения на Lyra/Pico. Это историческая
запись, обновлённый статус см. ниже.

## 10. Аппаратный статус на 1 ноября 2026 года (RK3506 / CPU0)

Протокол: [Lyra FreeRTOS Scheduler/Synchronization 2026-11-01](tests/lyra-freertos-20261101-scheduler.md).

- **KRN-S1.2: PASS (RK3506, CPU0).** Завершённые задачи освобождаются из task context независимо от создания следующей
  задачи; после `bench` free heap возвращается к базовому уровню (с точностью до малых постоянных выделений).
- **KRN-S2.1: PASS (RK3506, CPU0).** Event, Mutex, Semaphore и BusTransactionQueue; три конкурента подтверждают resource
  priority и FIFO (`expected=0,2,1`, `actual=0,2,1`).
- **KRN-S2.2: PASS (RK3506, CPU0).** 8 гонок освобождения шины/тайм-аута за прогон: 2 grants и 6 timeouts; повторные
  запуски не приводят к росту выделенной памяти.
- **KRN-S2.3: PASS (RK3506, CPU0).** Компактный профиль `bench --profile sync`: workspace 53 104 -> 16 796 байт, минимум
  свободного FreeRTOS heap 7 768 -> 44 072 байта; тесты продолжают проходить.

**Не закрыто:** общий IRQ-to-task контракт, отдельная реальная проверка `signalFromIsr`, приоритеты GIC для иных
источников, ISR ownership, SMP, FatFs concurrency, многодневный soak, SD/LCD DMA под FreeRTOS. Отдельная
экспериментальная проверка IRQ27 из `bench` не приравнивается к приёмке PL330 DMA.

## 11. Дополнение после аппаратных тестов 04.11.2026

| Область       | Проверенный результат                          | Не подтверждено                                            |
|---------------|------------------------------------------------|------------------------------------------------------------|
| FreeRTOS CPU0 | Shell, file programs, VFS/FatFs                | SMP, длительный конкурентный soak                          |
| Managed MMU   | WBWA данные + Normal-NC staging PL330          | Полная изоляция программ                                   |
| LCD DMA       | 119 chunks, 5280 КиБ/с, 0 faults/timeouts      | WFE=ON и долгий стресс                                     |
| SD DMA read   | 257 секторов, 965 КиБ/с, 0 faults/timeouts     | Скорость SD записи по этим данным                          |
| SD1 FAT RW    | `write`/`append`/`cp`/`sync`/RO remount прошли | Холодная проверка хешей, offline FAT check, аварии питания |
| Стек программ | `cp` 8192 Б, свободно 3896 Б                   | `write`/`append`/`cat` свободно 712/552/480 Б — запас мал  |

Указанное не закрывает KRN-S4 и проверки фактической конкурентной работы FatFs;
драйвер SD CMD24 остаётся PIO с readback, отдельным от SD DMA чтения.
