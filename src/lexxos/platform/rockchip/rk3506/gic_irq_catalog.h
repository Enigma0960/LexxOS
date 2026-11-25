#pragma once

#include <cstdint>

namespace lexxos::platform::rk3506 {

// The table is indexed by the GIC INTID, not by Linux GIC_SPI numbers.
// All implemented IDs are enumerated at runtime from GICD_TYPER. Unmapped
// names MUST NOT be guessed: SoC mux outputs may differ from Linux bindings.
inline constexpr std::uint32_t maxGicIntids = 1020U;

[[nodiscard]] constexpr char const* gicKind(std::uint32_t id) noexcept {
	return id < 16U ? "SGI" : id < 32U ? "PPI"
	                                   : "SPI";
}

[[nodiscard]] constexpr char const* gicKnownName(std::uint32_t id) noexcept {
	switch (id) {
		case 27U : return "CNTV timer";
		case 148U: return "DMAC0 IRQ0"; // RK3506 DTB GIC_SPI 116, event candidate.
		case 149U: return "DMAC0 IRQ1"; // DTB GIC_SPI 117; not enabled by LexxOS.
		default  : return "-"; // The line exists, but its SoC source is unknown.
	}
}

} // namespace lexxos::platform::rk3506
