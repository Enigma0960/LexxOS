#include <lexxos/app/console_input.h>

#include <lexxos/app/console_input_service.h>

namespace lexxos::app::console_input {

ConsoleInputService& defaultService() {
	static ConsoleInputService instance;
	return instance;
}

bool readChar(platform::PlatformServices const& platform, char& value) {
	return defaultService().readChar(platform, value);
}

std::size_t readAvailable(platform::PlatformServices const& platform, char* buffer, std::size_t capacity) {
	return defaultService().readAvailable(platform, buffer, capacity);
}

bool inject(std::string_view text) {
	return defaultService().inject(text);
}

void clearInjected() {
	return defaultService().clearInjected();
}

void setMode(Mode mode) {
	return defaultService().setMode(mode);
}

Mode mode() {
	return defaultService().mode();
}

std::size_t pendingInjected() {
	return defaultService().pendingInjected();
}

} // namespace lexxos::app::console_input
