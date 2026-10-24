#pragma once

#include <cstdint>
#include <string_view>

namespace lexxos::platform {
struct PlatformServices;
} // namespace lexxos::platform

namespace lexxos::devices::debug_display {

bool initialize();
bool initialize(lexxos::platform::PlatformServices const& platform);
[[nodiscard]] bool isReady();
void clear();
void writeLine(std::uint8_t line, std::string_view text);

} // namespace lexxos::devices::debug_display
