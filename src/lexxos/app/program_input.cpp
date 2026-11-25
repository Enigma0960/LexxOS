#include <lexxos/app/program_input.h>

#include <lexxos/app/program_input_service.h>

namespace lexxos::app::program_input {

ProgramInputService& defaultService() {
	static ProgramInputService instance{lifecycle::defaultEventBus()};
	return instance;
}

bool setInputMode(std::uint32_t processId, InputMode mode) {
	return defaultService().setInputMode(processId, mode);
}

InputMode inputMode(std::uint32_t processId) {
	return defaultService().inputMode(processId);
}

void releaseInputMode(std::uint32_t processId) {
	return defaultService().releaseInputMode(processId);
}

void publish(devices::InputEvent const& event) {
	return defaultService().publish(event);
}

void reset(EventCursor& cursor) {
	return defaultService().reset(cursor);
}

std::uint32_t sequence() {
	return defaultService().sequence();
}

bool pollEvent(EventCursor& cursor, devices::InputEvent& event) {
	return defaultService().pollEvent(cursor, event);
}

bool latestEvent(devices::InputEvent& event) {
	return defaultService().latestEvent(event);
}

} // namespace lexxos::app::program_input
