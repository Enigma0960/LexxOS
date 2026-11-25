/* RK3506 CPU0 PL1 Cortex-A7 PMUv2 diagnostic owner.
 * Counters are opt-in and never initialized from boot/IRQ paths.
 * PMU samples include interrupts and all other work executed on CPU0.
 */
#include <lexxos/platform/rockchip/rk3506/pmu_profile.h>

namespace lexxos::platform::rk3506 {

namespace {

	constexpr std::uint32_t kCounters = (1U << 31U) | 1U;
	constexpr std::uint32_t kInstructionEvent = 0x08U;
	constexpr std::uint32_t kPmcrEnable = 1U;
	bool g_owned{};
	char const* g_status{"not attempted"};

	PmuRegisterSnapshot g_snapshot{};

	std::uint32_t g_savedPmcr{};
	std::uint32_t g_savedSelector{};
	std::uint32_t g_savedEventType{};
	std::uint32_t g_savedEventCount{};
	std::uint32_t g_savedCycleCount{};

	extern "C" bool rk3506_pmu_probe(PmuRegisterSnapshot* snapshot);

	std::uint32_t readPmcr() {
		std::uint32_t v;
		asm volatile("mrc p15, 0, %0, c9, c12, 0" : "=r"(v));
		return v;
	}

	void writePmcr(std::uint32_t v) {
		asm volatile("mcr p15, 0, %0, c9, c12, 0; isb" : : "r"(v) : "memory");
	}

	std::uint32_t readEnabled() {
		std::uint32_t v;
		asm volatile("mrc p15, 0, %0, c9, c12, 1" : "=r"(v));
		return v;
	}

	void enable(std::uint32_t v) {
		asm volatile("mcr p15, 0, %0, c9, c12, 1" : : "r"(v) : "memory");
	}

	void disable(std::uint32_t v) {
		asm volatile("mcr p15, 0, %0, c9, c12, 2" : : "r"(v) : "memory");
	}

	std::uint32_t readOverflow() {
		std::uint32_t v;
		asm volatile("mrc p15, 0, %0, c9, c12, 3" : "=r"(v));
		return v;
	}

	void clearOverflow(std::uint32_t v) {
		asm volatile("mcr p15, 0, %0, c9, c12, 3" : : "r"(v) : "memory");
	}

	std::uint32_t readEventSupport() {
		std::uint32_t v;
		asm volatile("mrc p15, 0, %0, c9, c12, 6" : "=r"(v));
		return v;
	}

	std::uint32_t readSelector() {
		std::uint32_t v;
		asm volatile("mrc p15, 0, %0, c9, c12, 5" : "=r"(v));
		return v;
	}

	void select(std::uint32_t v) {
		asm volatile("mcr p15, 0, %0, c9, c12, 5; isb" : : "r"(v) : "memory");
	}

	std::uint32_t readEventType() {
		std::uint32_t v;
		asm volatile("mrc p15, 0, %0, c9, c13, 1" : "=r"(v));
		return v;
	}

	void writeEventType(std::uint32_t v) {
		asm volatile("mcr p15, 0, %0, c9, c13, 1" : : "r"(v) : "memory");
	}

	std::uint32_t readEventCount() {
		std::uint32_t v;
		asm volatile("mrc p15, 0, %0, c9, c13, 2" : "=r"(v));
		return v;
	}

	void writeEventCount(std::uint32_t v) {
		asm volatile("mcr p15, 0, %0, c9, c13, 2" : : "r"(v) : "memory");
	}

	std::uint32_t readCycleCount() {
		std::uint32_t v;
		asm volatile("mrc p15, 0, %0, c9, c13, 0" : "=r"(v));
		return v;
	}

	void writeCycleCount(std::uint32_t v) {
		asm volatile("mcr p15, 0, %0, c9, c13, 0" : : "r"(v) : "memory");
	}

} // namespace

bool pmuStart() noexcept {
	if (g_owned) {
		g_status = "already claimed by LexxOS";
		return false;
	}

	g_snapshot = {};
	if (!rk3506_pmu_probe(&g_snapshot)) {
		g_status = "denied or absent (CP15 PMU snapshot trapped; safe recovery)";
		return false;
	}

	g_snapshot.valid = true;
	std::uint32_t const pmcr = g_snapshot.pmcr;

	if (((pmcr >> 11U) & 0x1FU) < 1U) {
		g_status = "no programmable PMU event counters";
		return false;
	}

	// Boot firmware can leave PMCR.E set even though no counter is
	// enabled. Preserve the master state, but refuse on occupied counters,
	// armed interrupts or overflow flags. Never reset firmware PMU state.
	if (pmuOwnershipBlocked(g_snapshot)) {
		g_status = "busy (PMU ownership guard; inspect register snapshot)";
		return false;
	}

	if ((readEventSupport() & (1U << kInstructionEvent)) == 0U) {
		g_status = "instruction event 0x08 not supported";
		return false;
	}

	g_savedPmcr = pmcr;
	g_savedSelector = readSelector();
	select(0U);
	g_savedEventType = readEventType();
	g_savedEventCount = readEventCount();
	g_savedCycleCount = readCycleCount();

	writeEventType(kInstructionEvent);
	writeEventCount(0U);
	writeCycleCount(0U);
	asm volatile("isb" ::: "memory");
	g_owned = true;
	enable(kCounters);
	writePmcr((pmcr | kPmcrEnable) & ~(1U << 3U)); // Clear D: undivided CPU cycles; restore PMCR on stop.
	if ((readPmcr() & kPmcrEnable) == 0U || (readEnabled() & kCounters) != kCounters) {
		pmuStop();
		g_status = "PMU enable did not stick (restricted/RAZ-WI)";
		return false;
	}

	g_status = "enabled (CPU0 PMCCNTR + INST_EXEC 0x08)";
	return true;
}

PmuSample pmuSample() noexcept {
	if (!g_owned) {
		return {};
	}

	asm volatile("isb" ::: "memory");
	std::uint32_t const cycles = readCycleCount();
	std::uint32_t const instructions = readEventCount();
	std::uint32_t const overflow = readOverflow() & kCounters;
	if (overflow != 0U) {
		clearOverflow(overflow); // Next 128-sector sample starts with clean flags.
	}

	return {true, cycles, instructions, overflow};
}

void pmuStop() noexcept {
	if (!g_owned) {
		return;
	}

	// Disable first; restore only the state of the two exclusively claimed counters.
	disable(kCounters);
	writePmcr(g_savedPmcr);
	select(0U);
	writeEventType(g_savedEventType);
	writeEventCount(g_savedEventCount);
	writeCycleCount(g_savedCycleCount);
	clearOverflow(kCounters); // Our lines were clean before PMU was claimed.
	select(g_savedSelector);
	g_owned = false;
	g_status = "released";
}

char const* pmuStatus() noexcept {
	return g_status;
}

PmuRegisterSnapshot pmuRegisterSnapshot() noexcept {
	return g_snapshot;
}

} // namespace lexxos::platform::rk3506
