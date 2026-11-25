# IRQ Manager: первая переносимая реализация KRN-1

Статус: host-tested контракт/менеджер. Для RK3506 подготовлен первый opt-in
GIC adapter PPI 27; аппаратная приёмка ещё не выполнена, production fallback сохранён.
План: [IRQ/DMA/kernel roadmap](armv7a-irq-dma-kernel-roadmap.md).

## Разделение ответственности

- `kernel/irq.h` задаёт `IrqService`, `IrqSourceId`, `IrqRequest`, `IrqSubscription`.
- `kernel/irq_manager.h` реализует `IrqManager<Capacity>` без heap/RTOS,
  с фиксированным пулом, поколениями handles и подписками shared/exclusive.
- `IrqController` — требуемый платформенный адаптер. Он перечисляет домены,
  резервирует аппаратную линию, проверяет secure/firmware ownership,
  включает/отключает её и проводит безопасный teardown.
- CPU/SDK ISR entry/trampoline и ACK/EOI **не** находятся в менеджере.
  После декодирования аппаратного источника backend вызывает `dispatch()`;
  итог `IrqDisposition::Reschedule` обрабатывается на выходе из ISR.

`IrqVectorModel::Fixed` совместим с `runtimeSubscriptions`: фиксированный
вектор указывает на заранее слинкованный trampoline, а тот вызывает
`dispatch()` через регистрационный пул. Устанавливать новые адреса ISR
в ROM/NVIC/GIC ради отдельной подписки не требуется. SDK-managed и
relocatable-vector платформы могут подключаться к тому же сервису.

Каскадный домен (например, GPIO bank поверх одного GIC IRQ) объявляет
`IrqDomainInfo.parent`, но **распознавание дочернего источника и ACK**
остаётся делом backend; создание/удаление его parent route должно быть
согласовано с ownership контроллера. `IrqManager` не включает чужой IRQ.

## Инварианты

1. Все операции `IrqService` происходят только в task/init context.
   `IrqManager::dispatch()` вызывается только из ISR trampoline.
2. `beginUpdate` исключает dispatch на изменяемом пуле и дожидается
   уже идущих обработчиков до возврата. `endUpdate` восстанавливает
   исходное IRQ mask состояние. Backend без такой гарантии (особенно SMP)
   пока не может безопасно использовать менеджер.
3. `attachSource` вызывается один раз на физическую линию и оставляет её **выключенной**. Новый subscriber выключен даже
   на уже включённой
   shared-линии. Только переходы 0→1 и 1→0 enabled subscribers
   изменяют аппаратное состояние источника.
4. Shared-line callback вызывается для **всех** enabled subscribers;
   каждый проверяет/подтверждает собственный peripheral status.
   Несовместимые trigger, priority, affinity и exclusive/shared
   подписки отклоняются, настройки существующего владельца не меняются.
5. `detachSource` маскирует/освобождает **только свой** source, гарантирует
   отсутствие последующих callback и при ошибке сохраняет состояние.
   Менеджер удаляет подписку только при успешном завершении teardown.
6. Handle содержит slot+generation+source. Поколение не сбрасывается при
   удалении; исчерпанный `uint32_t` generation навсегда исключает слот из
   повторного использования, чтобы stale handle не стал валидным.
7. Менеджер не вызывает scheduler/VFS/malloc/логирование из ISR.
   `NotHandled` передаётся backend для политики spurious/storm/masking;
   `Reschedule` — подсказка для безопасного выхода из ISR, не context switch.
8. При отказе `attachSource`, `setLineEnabled` или `detachSource` состояние
   backend обязано остаться неизменным. Если контроллер не может обеспечить
   этот контракт, ему нужен собственный fail-closed recovery, **до** адаптера.
9. Менеджер и его пул должны жить дольше всех своих подписок и аппаратных
   trampoline; перед остановкой runtime нужно освободить все подписки.
   `owner` — идентификатор доверенного драйвера, не механизм авторизации
   пользовательских программ и не замена SDK capability checks.

## Следующий аппаратный срез

Первый opt-in backend `gic_irq_controller.*` обслуживает **только PPI 27**.
Он валидирует Group1, запрещает захват уже включённого чужого источника,
не сбрасывает GICD и использует CPU0 `cpsid i`/restore без включения IRQ,
если они были выключены. Подписка создаётся до изменения приоритета PPI.
Включение тестового маршрута: `-DLEXXOS_RK3506_MANAGED_TIMER_IRQ=ON`;
значение `OFF` сохраняет старый обработчик; DMAC0 использует INTID 148/149 из Lyra DTB.
На настоящей Lyra проверить PPI27 group1/enabled, 1 ms timer self-test,
загрузку shell, SWD/GDB RAM reload и cold/warm reboot. Не включать этот режим
по умолчанию до аппаратного parity.

Второй opt-in срез: `-DLEXXOS_RK3506_MANAGED_TIMER_IRQ=ON`
и `-DLEXXOS_RK3506_MANAGED_DMA_IRQ=ON`. IRQ148 подключается через
`IrqManager`, но только после явного `picosd dma ... wait irq` и успешного
probe; LCD PL330 и другой DMA-трафик не переключаются на IRQ. В GIC
контроллере IRQ148 проверяются доступный ISENABLER, level-trigger и CPU0
routing; недоступный Non-secure IGROUPR не используется. Контроллер
сохраняет/восстанавливает свой байт priority и ITARGETSR. При ошибке
подтверждения DMA148 маскируется до GICC EOI и требует release/reprobe.
IRQ149 (второй DMAC0 выход) пока **не захватывается и не включается**
менеджером: его назначение и ACK требуют отдельной аппаратной проверки.
Legacy аварийное маскирование 149 остаётся защитным fallback.

Аппаратная приёмка: холодная загрузка, `IRQ self-test: OK`, shell,
`sd-diag dma probe`, затем `sd-diag dma wait irq`, `sd-diag dma on`,
`sd-diag read 0` и сравнение с PIO,
повторные enable/disable/reprobe, GDB RAM reload и fault recovery.
При отказе возвращаться в polling, не пытаться продолжать частично
обработанную SD-команду. Если возникают зависания в IRQ148, выключить
флаг MANAGED_DMA_IRQ и сравнить прежний путь.

### Безопасность и диагностика маршрута DMA IRQ

`IrqService::securityInfo()` возвращает отдельно `executionWorld`, `group`,
`readable` и `configurable`. Для RK3506 CPU0, работающего в Non-secure PL1,
`group=Unknown`, `readable=false`, `configurable=false`. Чтение `IGROUPR`
здесь может вернуть RAZ/WI, поэтому ноль НЕ доказывает Group0. Это также
верно для legacy-диагностики IRQ148. Readback enable подтверждает действие,
но НЕ доказывает принадлежность Group1 или источник ограничения доступа.

В `sd-diag dma irq-info` доступны без изменения аппаратных регистров:
PL330 INTEN/INTSTATUS/FSRD/FSRC и снимки GIC IRQ27, 148 и 149 (ISENABLER, ISPENDR, ISACTIVER, ICFGR, priority, target,
GICD_CTLR, GICC_CTLR, GICC_PMR, GICC_HPPIR). IAR не читается и
EOI не выполняется. Вызвать до включения DMA: `dma on` может запустить
чтения через уже смонтированный FAT и спровоцировать timeout.

Если INTSTATUS=3, deliveries=0, проверить INTEN и GIC pending/enable:
при pending=0 событие не попадает в GIC; pending=1 указывает на
возможную проблему CPU target, PMR, CPU/distributor enable или security.
Одно мгновенное значение pending не доказывает причину; смотреть в динамике.
IRQ149 пока только наблюдается, НЕ подписывается и НЕ включается. После
подтверждения маршрута и abort-ACK на железе можно безопасно развивать backend.

### Recovery при потерянном IRQ148 (opt-in) / исторические измерения IRQ55

До исправления GIC55 enabled=1, CPU0 target=1, priority=0xA0,
PMR=0xF0; до DMA INTEN=0, это нормальное состояние. Ранее во время
передачи PL330 INTSTATUS=3 при 0 IRQ deliveries привёл к ложному
completion timeout и потере FAT-монтирования.

Теперь при `dma wait irq` обработчик ожидает sticky события ISR, но если
сам PL330 уже предъявил ВСЕ завершения (в том числе частично обслуженные),
сохраняет разовый снимок INTEN/ES/INTSTATUS, GRF_SOC_CON38/39 и GIC148,
отписывается от **своего** IRQ148 и завершает текущую DMA-передачу обычным
путём с проверкой channel idle и SPI FIFO. После этого дальнейшие передачи
работают в polling без нового автоматического захвата IRQ. Никогда не
считаем частичную DMA-передачу успешной, не делаем fallback при аппаратном
fault/overflow и не копируем данные до проверки idle. Cчётчик
`irqPollFallbacks` и `sd-diag dma irq-info` позволяют исследовать маршрут
после события, даже когда регистры PL330 уже очищены.

GRF маски читаются для диагностики; без подтверждения реальной схемы
маршрутизации ни один бит GRF не меняется. IRQ149 остаётся выключенным.

Следующая интеграция — IRQ149, после независимой проверки ownership
и безопасной обработки abort status на оборудовании.
Потребуется проверить Group1/OP-TEE ownership, обработку level/edge,
правильный ACK+EOI, размеры критической секции и cold/warm/SWD recovery.
Действующий `irq_timer.cpp` оставлять рабочим fallback до hardware parity.
Затем отдельные backends RP2040/RP2350 GPIO, STM32 NVIC/EXTI и ESP-IDF ISR
service, без переносов raw IRQ IDs между платформами.

Host-test: `lexxos_irq_manager_tests` проверяет регистрацию, shared IRQ,
сохранение векторного режима, cascade identity, ownership, capacity,
ошибки backend и generation-checked teardown.
