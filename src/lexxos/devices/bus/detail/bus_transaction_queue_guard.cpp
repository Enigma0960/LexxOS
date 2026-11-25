#include <lexxos/devices/bus/detail/bus_transaction_queue_guard.h>

#include <lexxos/devices/bus/bus_transaction_queue.h>

// Preserve cooperative RK3506 polling until its event backend is integrated.
#if defined(LEXXOS_EDITION_CE) || defined(LEXXOS_KERNEL_FREERTOS) || defined(LEXXOS_PLATFORM_ESPIDF) || (!defined(LEXXOS_PLATFORM_RK3506) && !defined(LEXXOS_PLATFORM_PICO) && !defined(LEXXOS_PLATFORM_STM32F4))
	#define LEXXOS_BUS_EVENT_WAIT 1
	#include <lexxos/kernel/synchronization.h>
#endif

#if !defined(LEXXOS_EDITION_CE) && defined(LEXXOS_PLATFORM_ESPIDF)
	#include <freertos/FreeRTOS.h>
	#include <freertos/task.h>
#elif !defined(LEXXOS_EDITION_CE) && (defined(LEXXOS_RK3506_FREERTOS_PERIPHERALS) || defined(LEXXOS_KERNEL_FREERTOS))
	#include <FreeRTOS.h>
	#include <task.h>
#endif

namespace lexxos::devices::detail {

BusTransactionQueueGuard::BusTransactionQueueGuard(BusTransactionQueue const& queue)
    : _queue(queue) {
#if !defined(LEXXOS_EDITION_CE) && (defined(LEXXOS_RK3506_FREERTOS_PERIPHERALS) || (defined(LEXXOS_KERNEL_FREERTOS) && !defined(LEXXOS_PLATFORM_ESPIDF)))
	taskENTER_CRITICAL();
#elif defined(LEXXOS_PLATFORM_RK3506)
	// RK3506 bring-up currently runs CPU0 cooperative tasks.
	// No task switch or bus IRQ takes place in this short region.
	_queue._guard = true;
#else
	while (_queue._guard.test_and_set(std::memory_order_acquire)) {
		// On preemptive RTOS a higher-priority waiter must not spin
		// forever while the preempted owner holds this short guard.
		if (auto wait = _queue.loadWaitFunction(); wait != nullptr) {
			wait(1);
		}
	}
#endif
}

BusTransactionQueueGuard::~BusTransactionQueueGuard() {
#if !defined(LEXXOS_EDITION_CE) && (defined(LEXXOS_RK3506_FREERTOS_PERIPHERALS) || (defined(LEXXOS_KERNEL_FREERTOS) && !defined(LEXXOS_PLATFORM_ESPIDF)))
	taskEXIT_CRITICAL();
#elif defined(LEXXOS_PLATFORM_RK3506)
	_queue._guard = false;
#else
	_queue._guard.clear(std::memory_order_release);
#endif
}

} // namespace lexxos::devices::detail
