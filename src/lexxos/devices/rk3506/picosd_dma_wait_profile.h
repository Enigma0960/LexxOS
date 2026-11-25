#pragma once

#include <cstdint>

namespace lexxos::platform::rk3506::detail {

// The virtual counter runs while WFE is entered. IRQ handlers, asynchronous
// preemption and immediate WFE returns are included in the measured interval.
// This is wall-clock accounting, NOT CPU active/idle cycle accounting.
[[nodiscard]] constexpr std::uint64_t dmaNonWfeWindowCycles(
    std::uint64_t elapsed, std::uint64_t irqWfe, std::uint64_t eventWfe) noexcept {
	auto const remaining = elapsed > irqWfe ? elapsed - irqWfe : 0ULL;
	return remaining > eventWfe ? remaining - eventWfe : 0ULL;
}

} // namespace lexxos::platform::rk3506::detail
