#pragma once

#include <array>
#include <atomic>

#include <lexxos/app/console_input.h>
#include <lexxos/runtime/selected_runtime_profile.h>

namespace lexxos::app {

// Один producer и один consumer; переключение режима не смешивает очереди разных OS.
class ConsoleInputService final {
public:
	using Mode = console_input::Mode;

	bool readChar(platform::PlatformServices const& platform, char& value);
	std::size_t readAvailable(platform::PlatformServices const& platform, char* buffer, std::size_t capacity);
	bool inject(std::string_view text);
	void clearInjected();
	void setMode(Mode mode);
	[[nodiscard]] Mode mode();
	[[nodiscard]] std::size_t pendingInjected();

private:
	std::size_t nextIndex(std::size_t index);
	bool pushInjected(char value);
	bool popInjected(char& value);

private:
	std::array<char, runtime::standardRuntimeProfile.shell.injectedInputBytes> _injected{};
	std::atomic<std::size_t> _head{0};
	std::atomic<std::size_t> _tail{0};
	std::atomic<std::uint8_t> _mode{static_cast<std::uint8_t>(Mode::Shell)};
};

} // namespace lexxos::app
