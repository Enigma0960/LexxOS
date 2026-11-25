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

namespace lexxos::devices {

void BusTransactionQueue::setWaitFunction(WaitFunction wait) {
#if defined(LEXXOS_PLATFORM_RK3506)
	_wait = wait;
#else
	_wait.store(wait, std::memory_order_release);
#endif
}

bool BusTransactionQueue::acquire(DeviceResourcePriority priority, std::uint32_t timeoutMs) {
	Request request{.priority = priority};
	[[maybe_unused]] WaitFunction wait = nullptr;
	{
		Guard guard{*this};
		if (!_active && _head == nullptr) {
			_active = true;
			return true;
		}

		wait = loadWaitFunction();

#if defined(LEXXOS_BUS_EVENT_WAIT)
	// Host preserves the legacy explicit opt-in; RTOS tasks can wait
	// without a platform-specific sleep callback.
	#if !defined(LEXXOS_KERNEL_FREERTOS) && !defined(LEXXOS_PLATFORM_ESPIDF)
		if (wait == nullptr) {
			return false;
		}
	#endif
#else
		if (wait == nullptr) {
			return false;
		}
#endif
		if (timeoutMs == 0U) {
			return false;
		}
	}

#if defined(LEXXOS_BUS_EVENT_WAIT)
	// Allocate the synchronization primitive only on contention, outside
	// the queue metadata guard; never allocate from an ISR.
	#if !defined(LEXXOS_EDITION_CE) && (defined(LEXXOS_KERNEL_FREERTOS) || defined(LEXXOS_PLATFORM_ESPIDF))
	if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
		return false;
	}
	#endif
	lexxos::kernel::Event event;
	if (!event.valid()) {
		return false;
	}

	request.event = &event;
#endif
	{
		Guard guard{*this};
		// The bus can become free during event construction.
		if (!_active && _head == nullptr) {
			_active = true;
			return true;
		}

		// Stable insertion: equal-priority requests retain FIFO order.
		auto** cursor = &_head;
		while (*cursor != nullptr
		    && static_cast<std::uint8_t>((*cursor)->priority)
		        >= static_cast<std::uint8_t>(priority)) {
			cursor = &(*cursor)->next;
		}

		request.next = *cursor;
		*cursor = &request;
		++_pending;
	}

#if defined(LEXXOS_BUS_EVENT_WAIT)
	// One bounded event wait, not timeoutMs iterations of sleep(1).
	// Completion and timeout race resolution both use the queue guard.
	auto const awakened = event.wait(timeoutMs);
	bool acquired = false;
	{
		Guard guard{*this};
		acquired = request.granted;
		if (!acquired) {
			auto** cursor = &_head;
			while (*cursor != nullptr && *cursor != &request) {
				cursor = &(*cursor)->next;
			}

			if (*cursor == &request) {
				*cursor = request.next;
				--_pending;
			}
		}
	}
	// A timeout can race with release() between selecting this request
	// and signalling it. Do not destroy the stack-owned Event until the
	// notifier is finished with it, including on preemptive/SMP systems.
	if (acquired) {
		if (!awakened) {
			(void) event.wait(lexxos::kernel::waitForever);
		}

		for (;;) {
			{
				Guard guard{*this};
				if (!request.notifying) {
					break;
				}
			}
			lexxos::kernel::synchronizationBackoff();
		}
	}

	return acquired;
#else
	// Cooperative fallback: retained until an RTOS-independent clock/event
	// backend is available. This measures sleep calls, not elapsed wall time.

	for (std::uint32_t elapsed = 0;; ++elapsed) {
		{
			Guard guard{*this};
			if (request.granted) {
				return true;
			}

			if (elapsed >= timeoutMs) {
				auto** cursor = &_head;
				while (*cursor != nullptr && *cursor != &request) {
					cursor = &(*cursor)->next;
				}

				if (*cursor == &request) {
					*cursor = request.next;
					--_pending;
				}

				return false;
			}
		}

		wait(1);
	}
#endif
}

void BusTransactionQueue::release() {
#if defined(LEXXOS_BUS_EVENT_WAIT)
	lexxos::kernel::Event* wake = nullptr;
	Request* selected = nullptr;
#endif
	{
		Guard guard{*this};
		if (!_active) {
			return;
		}

		if (_head == nullptr) {
			_active = false;
			return;
		}

		auto* const next = _head;
		_head = next->next;
		--_pending;
		next->next = nullptr;
		next->granted = true;
#if defined(LEXXOS_BUS_EVENT_WAIT)
		wake = next->event;
		selected = next;
		next->notifying = true;
#endif
		// Keep _active set while the selected waiter wakes up.
	}
#if defined(LEXXOS_BUS_EVENT_WAIT)
	// Never invoke an RTOS primitive while holding the queue guard. The
	// waiter retains its stack-owned Event until notifying becomes false.
	(void) wake->signal();
	{
		Guard guard{*this};
		selected->notifying = false;
	}
#endif
}

bool BusTransactionQueue::busy() const {
	Guard guard{*this};
	return _active;
}

std::uint32_t BusTransactionQueue::pending() const {
	Guard guard{*this};
	return _pending;
}

BusTransactionQueue::WaitFunction BusTransactionQueue::loadWaitFunction() const {
#if defined(LEXXOS_PLATFORM_RK3506)
	return _wait;
#else
	return _wait.load(std::memory_order_acquire);
#endif
}

} // namespace lexxos::devices
