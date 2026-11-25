#pragma once

#include <cstdint>

namespace lexxos::platform::rk3506 {

// Two 32-bit, CPU0-local counters; differences are modulo 2^32.
// A window with overflow is rejected rather than interpreted as a valid delta.
struct PmuSample {
	bool valid{};
	std::uint32_t cycles{};
	std::uint32_t instructions{};
	std::uint32_t overflow{};
};

struct PmuDelta {
	bool valid{};
	std::uint32_t cycles{};
	std::uint32_t instructions{};
};

[[nodiscard]] constexpr PmuDelta pmuDifference(PmuSample before, PmuSample after) noexcept {
	if (!before.valid || !after.valid || (before.overflow | after.overflow) != 0U) {
		return {};
	}

	return {true, after.cycles - before.cycles, after.instructions - before.instructions};
}

// Read-only snapshot captured by the guarded probe, before any PMU ownership.
// A set overflow flag might be stale; it is NOT proof of an active owner.
struct PmuRegisterSnapshot {
	std::uint32_t pmcr{};
	std::uint32_t enabled{}; // PMCNTENSET
	std::uint32_t interruptEnabled{}; // PMINTENSET
	std::uint32_t overflow{}; // PMOVSR
	std::uint32_t selector{}; // PMSELR
	bool valid{};
};

inline constexpr std::uint32_t pmuBusyGlobalEnable = 1U << 0U;
inline constexpr std::uint32_t pmuBusyCycleEnabled = 1U << 1U;
inline constexpr std::uint32_t pmuBusyEvent0Enabled = 1U << 2U;
inline constexpr std::uint32_t pmuBusyCycleInterrupt = 1U << 3U;
inline constexpr std::uint32_t pmuBusyEvent0Interrupt = 1U << 4U;
inline constexpr std::uint32_t pmuBusyCycleOverflow = 1U << 5U;
inline constexpr std::uint32_t pmuBusyEvent0Overflow = 1U << 6U;

[[nodiscard]] constexpr std::uint32_t pmuBusyReasons(PmuRegisterSnapshot const& snapshot) noexcept {
	constexpr std::uint32_t cycle = 1U << 31U;
	constexpr std::uint32_t event0 = 1U;
	return ((snapshot.pmcr & 1U) != 0U ? pmuBusyGlobalEnable : 0U)
	    | ((snapshot.enabled & cycle) != 0U ? pmuBusyCycleEnabled : 0U)
	    | ((snapshot.enabled & event0) != 0U ? pmuBusyEvent0Enabled : 0U)
	    | ((snapshot.interruptEnabled & cycle) != 0U ? pmuBusyCycleInterrupt : 0U)
	    | ((snapshot.interruptEnabled & event0) != 0U ? pmuBusyEvent0Interrupt : 0U)
	    | ((snapshot.overflow & cycle) != 0U ? pmuBusyCycleOverflow : 0U)
	    | ((snapshot.overflow & event0) != 0U ? pmuBusyEvent0Overflow : 0U);
}

// PMCR.E is the global master switch; it does not start any counter when
// PMCNTENSET is zero. An inherited E=1 alone is not an ownership conflict.
[[nodiscard]] constexpr bool pmuOwnershipBlocked(PmuRegisterSnapshot const& snapshot) noexcept {
	return !snapshot.valid || (pmuBusyReasons(snapshot) & ~pmuBusyGlobalEnable) != 0U;
}

// Explicitly opt-in; no PMU configuration during normal boot.
// Takes ownership only if the selected counters and interrupts are inactive.
// An inaccessible CP15 PMCR produces "denied" rather than a fatal undef.
[[nodiscard]] bool pmuStart() noexcept;
void pmuStop() noexcept;
[[nodiscard]] PmuSample pmuSample() noexcept;
[[nodiscard]] char const* pmuStatus() noexcept;
[[nodiscard]] PmuRegisterSnapshot pmuRegisterSnapshot() noexcept;

} // namespace lexxos::platform::rk3506
