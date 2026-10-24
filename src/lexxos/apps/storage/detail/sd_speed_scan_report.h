#pragma once

#include <cstdint>

#include <lexxos/apps/storage/detail/sd_speed_scan_types.h>
#include <lexxos/devices/bus/spi_bus.h>

namespace lexxos::os {
class AppContext;
} // namespace lexxos::os

namespace lexxos::apps::storage_programs::detail {

void writeSdSpeedScanConfiguration(
    lexxos::os::AppContext& context,
    SdSpeedScanConfig const& config,
    lexxos::devices::SpiClockMode targetMode);
void writeSdSpeedRawProbeReport(
    lexxos::os::AppContext& context,
    std::uint64_t bytesTotal,
    std::uint32_t elapsedMs,
    std::uint32_t referenceHash,
    std::uint32_t hashA,
    std::uint32_t hashB);
void writeSdSpeedFatProbeReport(
    lexxos::os::AppContext& context,
    char const* phase,
    std::uint64_t bytes,
    std::uint32_t elapsedMs);
void writeSdSpeedActiveBaud(lexxos::os::AppContext& context, char const* phase, std::uint32_t activeBaudRate);
void writeSdSpeedResult(lexxos::os::AppContext& context, std::uint32_t baudRate, bool stable);
void writeSdSpeedSelected(lexxos::os::AppContext& context, std::uint32_t baudRate);

} // namespace lexxos::apps::storage_programs::detail
