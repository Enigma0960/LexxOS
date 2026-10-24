#include <lexxos/app/detail/program_input_queue.h>

#if defined(LEXXOS_KERNEL_FREERTOS)
	#include <FreeRTOS.h>
	#include <task.h>
#endif

namespace lexxos::app::program_input::detail {

void ProgramInputQueue::lock() {
#if defined(LEXXOS_KERNEL_FREERTOS)
	taskENTER_CRITICAL();
#elif defined(LEXXOS_PLATFORM_HOST) || defined(LEXXOS_PLATFORM_SIM)
	_lock.lock();
#endif
}

void ProgramInputQueue::unlock() {
#if defined(LEXXOS_KERNEL_FREERTOS)
	taskEXIT_CRITICAL();
#elif defined(LEXXOS_PLATFORM_HOST) || defined(LEXXOS_PLATFORM_SIM)
	_lock.unlock();
#endif
}

std::uint32_t ProgramInputQueue::newestSequence() const {
	return _nextSequence > 1U ? _nextSequence - 1U : 0U;
}

std::uint32_t ProgramInputQueue::oldestAvailableSequence() const {
	auto const newest = newestSequence();
	if (newest == 0U) {
		return _nextSequence;
	}
	auto const capacity = lexxos::app::program_input::queueCapacity;
	return newest >= capacity ? newest - capacity + 1U : 1U;
}

void ProgramInputQueue::resetAfterSequenceWrap() {
	for (auto& sequence : _sequences) {
		sequence = 0;
	}
	for (auto& event : _events) {
		event = {};
	}
	_nextSequence = 1U;
}

void ProgramInputQueue::publish(lexxos::devices::InputEvent const& event) {
	lock();

	if (_nextSequence == 0U) {
		resetAfterSequenceWrap();
	}

	auto const sequence = _nextSequence++;
	auto const index = sequence % lexxos::app::program_input::queueCapacity;
	_sequences[index] = sequence;
	_events[index] = event;

	unlock();
}

void ProgramInputQueue::reset(EventCursor& cursor) {
	lock();
	cursor.nextSequence = _nextSequence;
	unlock();
}

std::uint32_t ProgramInputQueue::sequence() {
	lock();
	auto const output = newestSequence();
	unlock();
	return output;
}

bool ProgramInputQueue::pollEvent(EventCursor& cursor, lexxos::devices::InputEvent& event) {
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
			auto const index = requested % lexxos::app::program_input::queueCapacity;
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

bool ProgramInputQueue::latestEvent(lexxos::devices::InputEvent& event) {
	lock();

	bool hasEvent = false;
	auto const newest = newestSequence();
	if (newest != 0U) {
		auto const index = newest % lexxos::app::program_input::queueCapacity;
		if (_sequences[index] == newest) {
			event = _events[index];
			hasEvent = true;
		}
	}

	unlock();
	return hasEvent;
}

} // namespace lexxos::app::program_input::detail
