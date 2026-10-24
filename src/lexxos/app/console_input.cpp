#include <lexxos/app/console_input.h>

#include <array>
#include <atomic>

#include <lexxos/platform/platform_services.h>
#include <lexxos/runtime/runtime_profile.h>

namespace lexxos::app::console_input {
namespace {

	constexpr std::size_t injectedCapacity = lexxos::runtime::standardRuntimeProfile.shell.injectedInputBytes;

	std::array<char, injectedCapacity> g_injected{};
	std::atomic<std::size_t> g_head{0};
	std::atomic<std::size_t> g_tail{0};
	std::atomic<std::uint8_t> g_mode{static_cast<std::uint8_t>(Mode::Shell)};

	[[nodiscard]] std::size_t nextIndex(std::size_t index) {
		return (index + 1U) % g_injected.size();
	}

	[[nodiscard]] bool pushInjected(char value) {
		auto const tail = g_tail.load(std::memory_order_relaxed);
		auto const next = nextIndex(tail);
		if (next == g_head.load(std::memory_order_acquire)) {
			return false;
		}

		g_injected[tail] = value;
		g_tail.store(next, std::memory_order_release);
		return true;
	}

	[[nodiscard]] bool popInjected(char& value) {
		auto const head = g_head.load(std::memory_order_relaxed);
		if (head == g_tail.load(std::memory_order_acquire)) {
			return false;
		}

		value = g_injected[head];
		g_head.store(nextIndex(head), std::memory_order_release);
		return true;
	}

} // namespace

bool readChar(lexxos::platform::PlatformServices const& platform, char& value) {
	if (popInjected(value)) {
		return true;
	}
	return platform.console.readChar != nullptr && platform.console.readChar(value);
}

std::size_t readAvailable(lexxos::platform::PlatformServices const& platform, char* buffer, std::size_t capacity) {
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

bool inject(std::string_view text) {
	auto const head = g_head.load(std::memory_order_acquire);
	auto const tail = g_tail.load(std::memory_order_acquire);
	auto const available = tail >= head ? (g_injected.size() - (tail - head) - 1U) : (head - tail - 1U);
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

void clearInjected() {
	g_head.store(g_tail.load(std::memory_order_acquire), std::memory_order_release);
}

void setMode(Mode mode) {
	g_mode.store(static_cast<std::uint8_t>(mode), std::memory_order_release);
}

Mode mode() {
	return static_cast<Mode>(g_mode.load(std::memory_order_acquire));
}

std::size_t pendingInjected() {
	auto const head = g_head.load(std::memory_order_acquire);
	auto const tail = g_tail.load(std::memory_order_acquire);
	return tail >= head ? (tail - head) : (g_injected.size() - head + tail);
}

} // namespace lexxos::app::console_input
