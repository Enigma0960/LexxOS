#include <lexxos/app/program_input_service.h>

#if defined(LEXXOS_KERNEL_FREERTOS)
	#include <FreeRTOS.h>
	#include <task.h>
#endif

namespace lexxos::app {

ProgramInputService::ProgramInputService(lifecycle::EventBus& events)
    : _events(events) {
}

lifecycle::EventBus& ProgramInputService::events() {
	return _events;
}

void ProgramInputService::lockModes() {
#if defined(LEXXOS_KERNEL_FREERTOS)
	taskENTER_CRITICAL();
#elif defined(LEXXOS_PLATFORM_HOST) || defined(LEXXOS_PLATFORM_SIM)
	_processModesMutex.lock();
#endif
}

void ProgramInputService::unlockModes() {
#if defined(LEXXOS_KERNEL_FREERTOS)
	taskEXIT_CRITICAL();
#elif defined(LEXXOS_PLATFORM_HOST) || defined(LEXXOS_PLATFORM_SIM)
	_processModesMutex.unlock();
#endif
}

bool ProgramInputService::setInputMode(std::uint32_t processId, InputMode mode) {
	if (processId == 0U || (mode != InputMode::Terminal && mode != InputMode::Events)) {
		return false;
	}

	lockModes();
	ProcessMode* empty = nullptr;
	for (auto& entry : _processModes) {
		if (entry.id == processId) {
			entry.mode = mode;
			unlockModes();
			return true;
		}

		if (entry.id == 0U && empty == nullptr) {
			empty = &entry;
		}
	}

	if (empty != nullptr) {
		*empty = {processId, mode};
	}

	unlockModes();
	return empty != nullptr;
}

ProgramInputService::InputMode ProgramInputService::inputMode(std::uint32_t processId) {
	lockModes();
	for (auto const& entry : _processModes) {
		if (entry.id != 0U && entry.id == processId) {
			auto const mode = entry.mode;
			unlockModes();
			return mode;
		}
	}

	unlockModes();
	return InputMode::Terminal;
}

void ProgramInputService::releaseInputMode(std::uint32_t processId) {
	if (processId == 0U) {
		return;
	}

	lockModes();
	for (auto& entry : _processModes) {
		if (entry.id == processId) {
			entry = {};
			break;
		}
	}

	unlockModes();
}

void ProgramInputService::publish(lexxos::devices::InputEvent const& event) {
	_queue.publish(event);
	(void) _events.publish({
	    .type = lexxos::lifecycle::EventType::Input,
	    .timestampMs = event.timestampMs,
	    .input = event,
	});
}

void ProgramInputService::reset(EventCursor& cursor) {
	_queue.reset(cursor);
}

std::uint32_t ProgramInputService::sequence() {
	return _queue.sequence();
}

bool ProgramInputService::pollEvent(EventCursor& cursor, lexxos::devices::InputEvent& event) {
	return _queue.pollEvent(cursor, event);
}

bool ProgramInputService::latestEvent(lexxos::devices::InputEvent& event) {
	return _queue.latestEvent(event);
}

} // namespace lexxos::app
