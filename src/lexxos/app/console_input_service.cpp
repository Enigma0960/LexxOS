#include <lexxos/app/console_input_service.h>

#include <lexxos/platform/platform_services.h>

namespace lexxos::app {

[[nodiscard]] std::size_t ConsoleInputService::nextIndex(std::size_t index) {
	return (index + 1U) % _injected.size();
}

[[nodiscard]] bool ConsoleInputService::pushInjected(char value) {
	auto const tail = _tail.load(std::memory_order_relaxed);
	auto const next = nextIndex(tail);
	if (next == _head.load(std::memory_order_acquire)) {
		return false;
	}

	_injected[tail] = value;
	_tail.store(next, std::memory_order_release);
	return true;
}

[[nodiscard]] bool ConsoleInputService::popInjected(char& value) {
	auto const head = _head.load(std::memory_order_relaxed);
	if (head == _tail.load(std::memory_order_acquire)) {
		return false;
	}

	value = _injected[head];
	_head.store(nextIndex(head), std::memory_order_release);
	return true;
}

bool ConsoleInputService::readChar(lexxos::platform::PlatformServices const& platform, char& value) {
	if (popInjected(value)) {
		return true;
	}
	return platform.console.readChar != nullptr && platform.console.readChar(value);
}

std::size_t ConsoleInputService::readAvailable(lexxos::platform::PlatformServices const& platform, char* buffer, std::size_t capacity) {
	if (buffer == nullptr || capacity == 0) {
		return 0;
	}

	std::size_t count = 0;
	while (count < capacity && popInjected(buffer[count])) {
		++count;
	}

	if (count < capacity && platform.console.readAvailable != nullptr) {
		count += platform.console.readAvailable(buffer + count, capacity - count);
		return count;
	}

	if (count == 0 && platform.console.readChar != nullptr && platform.console.readChar(buffer[count])) {
		++count;
	}

	return count;
}

bool ConsoleInputService::inject(std::string_view text) {
	auto const head = _head.load(std::memory_order_acquire);
	auto const tail = _tail.load(std::memory_order_acquire);
	auto const available = tail >= head ? (_injected.size() - (tail - head) - 1U) : (head - tail - 1U);
	if (text.size() > available) {
		return false;
	}

	for (char const value : text) {
		if (!pushInjected(value)) {
			return false;
		}
	}

	return true;
}

void ConsoleInputService::clearInjected() {
	_head.store(_tail.load(std::memory_order_acquire), std::memory_order_release);
}

void ConsoleInputService::setMode(Mode mode) {
	_mode.store(static_cast<std::uint8_t>(mode), std::memory_order_release);
}

ConsoleInputService::Mode ConsoleInputService::mode() {
	return static_cast<Mode>(_mode.load(std::memory_order_acquire));
}

std::size_t ConsoleInputService::pendingInjected() {
	auto const head = _head.load(std::memory_order_acquire);
	auto const tail = _tail.load(std::memory_order_acquire);
	return tail >= head ? (tail - head) : (_injected.size() - head + tail);
}

} // namespace lexxos::app
