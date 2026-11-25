#pragma once

#include <span>

#include <lexxos/platform/board.h>

namespace lexxos::platform {

struct PlatformServices;

} // namespace lexxos::platform

namespace lexxos::devices {

class DeviceManager;

} // namespace lexxos::devices

namespace lexxos::composition {

// Провайдеры вызываются по порядку после GPIO binding, до настройки арбитража шин.
using BoardDeviceProvider = void (*)(devices::DeviceManager&, platform::PlatformServices const&,
    platform::LegacyBoardDescriptor const&);

bool registerBoardDevices(devices::DeviceManager& devices, platform::PlatformServices const& platformServices,
    std::span<BoardDeviceProvider const> providers);

bool registerBoardDevices(lexxos::devices::DeviceManager& devices, lexxos::platform::PlatformServices const& platformServices);

} // namespace lexxos::composition
