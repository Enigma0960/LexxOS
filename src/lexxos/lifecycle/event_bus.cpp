#include <lexxos/lifecycle/event_bus.h>

#if defined(LEXXOS_EDITION_CE)
	#include <lexxos/kernel/synchronization.h>
#elif defined(LEXXOS_KERNEL_ESPIDF)
	#include <freertos/task.h>
#elif defined(LEXXOS_KERNEL_FREERTOS)
	#include <FreeRTOS.h>
	#include <task.h>
#endif

namespace lexxos::lifecycle {

EventBus& defaultEventBus() {
	static EventBus instance;
	return instance;
}

void EventBus::lock() {
#if defined(LEXXOS_EDITION_CE)
	while (_guard.test_and_set(std::memory_order_acquire)) {
		lexxos::kernel::synchronizationBackoff();
	}
#elif defined(LEXXOS_KERNEL_ESPIDF)
	taskENTER_CRITICAL(&_mux);
#elif defined(LEXXOS_KERNEL_FREERTOS)
	taskENTER_CRITICAL();
#elif defined(LEXXOS_PLATFORM_HOST) || defined(LEXXOS_PLATFORM_SIM)
	_lock.lock();
#endif
}

void EventBus::unlock() {
#if defined(LEXXOS_EDITION_CE)
	_guard.clear(std::memory_order_release);
#elif defined(LEXXOS_KERNEL_ESPIDF)
	taskEXIT_CRITICAL(&_mux);
#elif defined(LEXXOS_KERNEL_FREERTOS)
	taskEXIT_CRITICAL();
#elif defined(LEXXOS_PLATFORM_HOST) || defined(LEXXOS_PLATFORM_SIM)
	_lock.unlock();
#endif
}

std::uint32_t EventBus::newestSequence() const {
	return _nextSequence > 1U ? _nextSequence - 1U : 0U;
}

std::uint32_t EventBus::oldestAvailableSequence() const {
	auto const newest = newestSequence();
	if (newest == 0U) {
		return _nextSequence;
	}
	return newest >= queueCapacity ? newest - queueCapacity + 1U : 1U;
}

void EventBus::resetAfterSequenceWrap() {
	for (auto& sequence : _sequences) {
		sequence = 0;
	}

	for (auto& event : _events) {
		event = {};
	}

	_nextSequence = 1U;
}

bool EventBus::pushUnlocked(Event const& event) {
	if (_nextSequence == 0U) {
		resetAfterSequenceWrap();
	}

	auto const sequence = _nextSequence++;
	auto const index = sequence % queueCapacity;
	_sequences[index] = sequence;
	_events[index] = event;
	return true;
}

bool EventBus::publish(Event const& event) {
	lock();
	bool const pushed = pushUnlocked(event);
	unlock();
	return pushed;
}

bool EventBus::publishFromIsr(Event const& event) {
#if defined(LEXXOS_EDITION_CE)
	if (_guard.test_and_set(std::memory_order_acquire)) {
		return false;
	}

	bool const pushed = pushUnlocked(event);
	_guard.clear(std::memory_order_release);
	return pushed;
#elif defined(LEXXOS_KERNEL_ESPIDF)
	taskENTER_CRITICAL_ISR(&_mux);
	bool const pushed = pushUnlocked(event);
	taskEXIT_CRITICAL_ISR(&_mux);
	return pushed;
#elif defined(LEXXOS_KERNEL_FREERTOS)
	UBaseType_t const previousMask = taskENTER_CRITICAL_FROM_ISR();
	bool const pushed = pushUnlocked(event);
	taskEXIT_CRITICAL_FROM_ISR(previousMask);
	return pushed;
#else
	return publish(event);
#endif
}

void EventBus::reset(EventCursor& cursor) {
	lock();
	cursor.nextSequence = _nextSequence;
	unlock();
}

std::uint32_t EventBus::sequence() {
	lock();
	auto const output = newestSequence();
	unlock();
	return output;
}

bool EventBus::poll(EventCursor& cursor, Event& event) {
	lock();

	bool hasEvent = false;
	auto const newest = newestSequence();
	if (newest == 0U) {
		cursor.nextSequence = _nextSequence;
	} else {
		auto const oldest = oldestAvailableSequence();
		if (cursor.nextSequence == 0U || cursor.nextSequence < oldest) {
			cursor.nextSequence = oldest;
		}

		if (cursor.nextSequence > newest) {
			cursor.nextSequence = _nextSequence;
		} else {
			auto const requested = cursor.nextSequence;
			auto const index = requested % queueCapacity;
			if (_sequences[index] == requested) {
				event = _events[index];
				cursor.nextSequence = requested + 1U;
				hasEvent = true;
			} else {
				cursor.nextSequence = _nextSequence;
			}
		}
	}

	unlock();
	return hasEvent;
}

bool EventBus::latest(Event& event) {
	lock();

	bool hasEvent = false;
	auto const newest = newestSequence();
	if (newest != 0U) {
		auto const index = newest % queueCapacity;
		if (_sequences[index] == newest) {
			event = _events[index];
			hasEvent = true;
		}
	}

	unlock();
	return hasEvent;
}

} // namespace lexxos::lifecycle
