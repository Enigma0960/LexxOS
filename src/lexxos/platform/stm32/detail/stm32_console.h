#pragma once

#include <cstddef>
#include <string_view>

#include <lexxos/platform/board.h>

namespace lexxos::platform::stm32::detail {

void initializeConsole(UartPins const& pins);
void writeConsole(std::string_view text);
[[nodiscard]] bool readConsoleChar(char& value);
[[nodiscard]] std::size_t readConsoleAvailable(char* buffer, std::size_t capacity);

} // namespace lexxos::platform::stm32::detail
