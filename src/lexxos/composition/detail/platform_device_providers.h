#pragma once

#include <lexxos/composition/board_device_providers.h>

namespace lexxos::composition::detail {

platform::LegacyBoardDescriptor const& boardFor(platform::PlatformServices const& platform);

// Реализацию задаёт ровно один target-local исходник, выбранный сборкой.
std::span<BoardDeviceProvider const> selectedBoardDeviceProviders();

// Явно подключаемое legacy wiring; native-продукты задают собственные шины/носители.
void registerLegacyBoardDevices(devices::DeviceManager& devices,
    platform::PlatformServices const& platformServices, platform::LegacyBoardDescriptor const& board);

} // namespace lexxos::composition::detail
