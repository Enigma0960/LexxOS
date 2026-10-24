#pragma once

#include <cstdint>

namespace lexxos::os {
class AppContext;
} // namespace lexxos::os

namespace lexxos::apps::storage_programs::detail {

[[nodiscard]] bool runSpeedFatWriteProbe(
    lexxos::os::AppContext& context,
    std::uint32_t blocksPerFile,
    std::uint32_t files,
    std::uint32_t& elapsedMsOut);
[[nodiscard]] bool runSpeedFatReadProbe(
    lexxos::os::AppContext& context,
    std::uint32_t blocksPerFile,
    std::uint32_t files,
    std::uint32_t& elapsedMsOut);

} // namespace lexxos::apps::storage_programs::detail
