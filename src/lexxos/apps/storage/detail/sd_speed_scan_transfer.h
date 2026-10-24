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

[[nodiscard]] bool setSpeedScanTransfer(
    lexxos::os::AppContext& context,
    lexxos::devices::SpiClockMode mode,
    std::uint32_t baudRate,
    char const* phase);
[[nodiscard]] bool readBlocksAtSettings(
    lexxos::os::AppContext& context,
    lexxos::devices::BlockDevice& device,
    lexxos::devices::SpiClockMode mode,
    std::uint32_t expectedBaudRate,
    std::uint64_t firstBlock,
    std::uint32_t blockCount,
    std::uint8_t* output,
    char const* phase);
[[nodiscard]] bool recoverSpeedScanReference(
    lexxos::os::AppContext& context,
    lexxos::devices::BlockDevice& device,
    char const* phase);
[[nodiscard]] bool cleanupSpeedFilesInReferenceMode(lexxos::os::AppContext& context, std::uint32_t files);

} // namespace lexxos::apps::storage_programs::detail
