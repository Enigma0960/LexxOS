#pragma once

#include <lexxos/composition/board_device_providers.h>
#include <lexxos/platform/board.h>

namespace lexxos::devices {
class I2cBus;
}

namespace lexxos::composition::detail {

lexxos::platform::BoardDescriptor const& boardFor(lexxos::platform::PlatformServices const& platform);
void registerPicoBoardDevices(
    lexxos::devices::DeviceManager& devices,
    lexxos::platform::PlatformServices const& platformServices,
    lexxos::platform::BoardDescriptor const& board);

void registerStm32BoardDevices(
    lexxos::devices::DeviceManager& devices,
    lexxos::platform::PlatformServices const& platformServices,
    lexxos::platform::BoardDescriptor const& activeBoard);

void registerEspidfBoardDevices(
    lexxos::devices::DeviceManager& devices,
    lexxos::platform::PlatformServices const& platformServices,
    lexxos::platform::BoardDescriptor const& activeBoard,
    lexxos::devices::I2cBus& debugBus);

void registerSimulatorDevices(lexxos::devices::DeviceManager& devices);

} // namespace lexxos::composition::detail
