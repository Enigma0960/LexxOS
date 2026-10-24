#pragma once

#include <cstdint>
#include <string_view>

#include <lexxos/devices/bus/spi_bus.h>

namespace lexxos::apps::storage_programs::detail {

[[nodiscard]] std::uint32_t sdSpeedMinimumBaudRate();
[[nodiscard]] std::uint32_t sdSafeBaudRate();
[[nodiscard]] std::uint32_t sdStableBaudRate();
[[nodiscard]] std::uint32_t sdPerformanceBaudRate();
[[nodiscard]] std::uint32_t sdMaximumBaudRate();

[[nodiscard]] bool parseSpiMode(std::string_view text, lexxos::devices::SpiClockMode& output);
[[nodiscard]] bool parseOnOff(std::string_view text, bool& output);

} // namespace lexxos::apps::storage_programs::detail
