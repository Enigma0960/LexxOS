#pragma once

#include <cstdint>

#include <lexxos/os/app_context.h>
#include <lexxos/platform/board.h>

namespace lexxos::apps::picocalc_programs::detail {

inline constexpr std::uint32_t minBeepFrequencyHz = 20;
inline constexpr std::uint32_t maxBeepFrequencyHz = 20000;
inline constexpr std::uint32_t maxBeepDurationMs = 5000;

[[nodiscard]] int runBeepSequenceOnPico(
    lexxos::os::AppContext& context,
    lexxos::platform::AudioBeepPins const& config,
    std::uint32_t frequencyHz,
    std::uint32_t durationMs);
} // namespace lexxos::apps::picocalc_programs::detail
