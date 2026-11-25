#pragma once

#include <cstdint>

namespace lexxos::platform::rk3506::detail {

// A missed IRQ may be recovered ONLY after all expected PL330 events have
// completed. A partial event mask must never be accepted as completion.
[[nodiscard]] constexpr bool dmaEventsCompletedWithoutIrq(
    std::uint32_t receivedByIsr, std::uint32_t pendingInPl330, std::uint32_t expected) noexcept {
	return expected != 0U
	    && (receivedByIsr & expected) != expected
	    && ((receivedByIsr | pendingInPl330) & expected) == expected;
}

} // namespace lexxos::platform::rk3506::detail
