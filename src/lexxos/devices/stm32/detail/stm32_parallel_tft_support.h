#pragma once

#include <cstdint>

#include <lexxos/devices/display_device.h>
#include <lexxos/platform/board.h>

namespace lexxos::platform {
struct PlatformServices;
} // namespace lexxos::platform

namespace lexxos::devices::stm32::detail {

[[nodiscard]] bool configureParallelTftBus(lexxos::platform::ParallelTftDisplayPins const& config);
void writeParallelTftCommand(std::uint16_t value);
void writeParallelTftData(std::uint16_t value);
void initializeIli9341();
void initializeIli9341(lexxos::platform::PlatformServices const& platform);
void setIli9341AddressWindow(Rect const& rect);

} // namespace lexxos::devices::stm32::detail
