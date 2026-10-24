#pragma once

#include <cstdint>

#include <lexxos/devices/bus/spi_bus.h>

namespace lexxos::devices {
class BlockDevice;
} // namespace lexxos::devices

namespace lexxos::os {
class AppContext;
} // namespace lexxos::os

namespace lexxos::apps::storage_programs::detail {

[[nodiscard]] bool runSpeedRawProbe(
    lexxos::os::AppContext& context,
    lexxos::devices::BlockDevice& device,
    lexxos::devices::SpiClockMode targetMode,
    std::uint32_t expectedBaudRate,
    std::uint64_t firstBlock,
    std::uint32_t blocks,
    std::uint32_t rawBatchBlocks,
    bool& stable);

} // namespace lexxos::apps::storage_programs::detail
