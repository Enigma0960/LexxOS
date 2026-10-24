#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace lexxos::platform {
struct PlatformServices;
}

namespace lexxos::app::console_input {

enum class Mode : std::uint8_t {
	Shell,
	Program,
	RawProgram,
};

bool readChar(lexxos::platform::PlatformServices const& platform, char& value);
std::size_t readAvailable(lexxos::platform::PlatformServices const& platform, char* buffer, std::size_t capacity);
bool inject(std::string_view text);
void clearInjected();

void setMode(Mode mode);
[[nodiscard]] Mode mode();
[[nodiscard]] std::size_t pendingInjected();

} // namespace lexxos::app::console_input
