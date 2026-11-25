#pragma once

#include <lexxos/composition/board_device_providers.h>
#include <lexxos/devices/display/double_buffered_display.h>
#include <lexxos/devices/display/ili9488_display.h>
#include <lexxos/hardware/products/lyra_endpoints.h>

namespace lexxos::devices {

class I2cBus;

} // namespace lexxos::devices

namespace lexxos::composition::products {

bool startPicoCalcLyraDisplayTask();

// CPU0 snapshot; safe after the caller waits for the pending frame.
devices::DoubleBufferedDisplayTiming picoCalcLyraDisplayTiming();
devices::Ili9488DisplayTiming picoCalcLyraPanelTiming();

void registerPicoCalcLyraInput(devices::DeviceManager& devices,
    platform::PlatformServices const& services, devices::I2cBus& bus);
void registerPicoCalcLyraDisplay(devices::DeviceManager& devices,
    platform::PlatformServices const& services, hardware::products::LyraDisplayEndpoint const& endpoint, std::uint32_t frequencyHz);

} // namespace lexxos::composition::products
