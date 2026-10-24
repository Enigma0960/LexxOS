#pragma once

namespace lexxos::platform {
struct PlatformServices;
} // namespace lexxos::platform

namespace lexxos::devices {

class DeviceManager;

} // namespace lexxos::devices

namespace lexxos::composition {

void registerBoardDevices(lexxos::devices::DeviceManager& devices, lexxos::platform::PlatformServices const& platformServices);

} // namespace lexxos::composition
