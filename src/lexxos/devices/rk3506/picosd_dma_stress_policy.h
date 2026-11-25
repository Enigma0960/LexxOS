#pragma once

#include <lexxos/devices/rk3506/picosd_dma_pl330.h>

namespace lexxos::platform::rk3506::detail {

// Counter deltas are modulo 2^32; no assumption of one IRQ per sector.
// A single GIC interrupt may acknowledge both PL330 event bits.
[[nodiscard]] constexpr bool dmaStressPassOk(PicoSdDmaStats const& before,
    PicoSdDmaStats const& after, std::uint32_t sectors, bool irqMode) noexcept {
	return (after.reads - before.reads) == sectors
	    && (after.errors - before.errors) == 0U
	    && (after.timeouts - before.timeouts) == 0U
	    && (after.irqPollFallbacks - before.irqPollFallbacks) == 0U
	    && (after.irqEmptyInterrupts - before.irqEmptyInterrupts) == 0U
	    && (!irqMode || (after.irqInterrupts - before.irqInterrupts) != 0U);
}

} // namespace lexxos::platform::rk3506::detail
