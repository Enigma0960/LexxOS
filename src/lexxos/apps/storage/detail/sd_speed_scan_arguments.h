#pragma once

#include <cstdint>

#include <lexxos/apps/storage/detail/sd_speed_scan_types.h>
#include <lexxos/program/program.h>

namespace lexxos::os {
class AppContext;
} // namespace lexxos::os

namespace lexxos::program {
class ParsedArguments;
} // namespace lexxos::program

namespace lexxos::apps::storage_programs::detail {

[[nodiscard]] lexxos::program::ProgramArgumentList sdSpeedScanArgumentList();
[[nodiscard]] bool parseSdSpeedScanConfig(
    lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments,
    SdSpeedScanConfig& config);
[[nodiscard]] bool resolveSdSpeedScanRawStart(
    lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments,
    std::uint64_t deviceBlockCount,
    SdSpeedScanConfig& config);
void writeSdSpeedScanUsage(lexxos::os::AppContext& context);

} // namespace lexxos::apps::storage_programs::detail
