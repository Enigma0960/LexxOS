#include <cassert>
#include <cstdint>

#include <lexxos/devices/rk3506/picosd_dma_irq_wait.h>
#include <lexxos/devices/rk3506/picosd_dma_stress_policy.h>
#include <lexxos/devices/rk3506/picosd_dma_wait_profile.h>
#include <lexxos/platform/rockchip/rk3506/pmu_profile.h>

int main() {
	using lexxos::platform::rk3506::detail::dmaEventsCompletedWithoutIrq;
	static_assert(!dmaEventsCompletedWithoutIrq(0U, 0U, 3U));
	static_assert(!dmaEventsCompletedWithoutIrq(0U, 1U, 3U));
	static_assert(!dmaEventsCompletedWithoutIrq(1U, 0U, 3U));
	static_assert(dmaEventsCompletedWithoutIrq(0U, 3U, 3U));
	static_assert(dmaEventsCompletedWithoutIrq(1U, 2U, 3U));
	static_assert(dmaEventsCompletedWithoutIrq(2U, 1U, 3U));
	static_assert(!dmaEventsCompletedWithoutIrq(3U, 0U, 3U));
	static_assert(!dmaEventsCompletedWithoutIrq(0U, 0U, 0U));

	for (std::uint32_t isr = 0; isr != 4U; ++isr) {
		for (std::uint32_t raw = 0; raw != 4U; ++raw) {
			bool const expected = isr != 3U && (isr | raw) == 3U;
			assert(dmaEventsCompletedWithoutIrq(isr, raw, 3U) == expected);
		}
	}

	using lexxos::platform::rk3506::detail::dmaStressPassOk;
	using lexxos::platform::rk3506::PicoSdDmaStats;

	PicoSdDmaStats before{}, after{};
	before.reads = 30U;
	before.irqInterrupts = 10U;
	after.reads = 158U;
	after.irqInterrupts = 11U;
	static_assert(dmaStressPassOk(PicoSdDmaStats{}, PicoSdDmaStats{.reads = 128U, .irqInterrupts = 1U}, 128U, true));
	assert(dmaStressPassOk(before, after, 128U, true));
	after.irqInterrupts = 10U;
	assert(!dmaStressPassOk(before, after, 128U, true));
	assert(dmaStressPassOk(before, after, 128U, false));
	after.irqPollFallbacks = 1U;
	assert(!dmaStressPassOk(before, after, 128U, false));
	after.irqPollFallbacks = 0U;
	after.errors = 1U;
	assert(!dmaStressPassOk(before, after, 128U, false));

	using lexxos::platform::rk3506::detail::dmaNonWfeWindowCycles;
	static_assert(dmaNonWfeWindowCycles(100U, 0U, 0U) == 100U);
	static_assert(dmaNonWfeWindowCycles(100U, 80U, 0U) == 20U);
	static_assert(dmaNonWfeWindowCycles(100U, 20U, 70U) == 10U);
	static_assert(dmaNonWfeWindowCycles(100U, 101U, 0U) == 0U);
	static_assert(dmaNonWfeWindowCycles(100U, 70U, 60U) == 0U);
	assert(dmaNonWfeWindowCycles(24000000ULL, 18000000ULL, 0ULL) == 6000000ULL);

	using lexxos::platform::rk3506::pmuBusyCycleEnabled;
	using lexxos::platform::rk3506::pmuBusyCycleInterrupt;
	using lexxos::platform::rk3506::pmuBusyCycleOverflow;
	using lexxos::platform::rk3506::pmuBusyEvent0Enabled;
	using lexxos::platform::rk3506::pmuBusyEvent0Interrupt;
	using lexxos::platform::rk3506::pmuBusyEvent0Overflow;
	using lexxos::platform::rk3506::pmuBusyGlobalEnable;
	using lexxos::platform::rk3506::pmuBusyReasons;
	using lexxos::platform::rk3506::PmuRegisterSnapshot;
	static_assert(pmuBusyReasons({}) == 0U);
	static_assert(pmuBusyReasons(PmuRegisterSnapshot{.pmcr = 1U}) == pmuBusyGlobalEnable);
	static_assert(pmuBusyReasons(PmuRegisterSnapshot{.enabled = 0x80000001U}) == (pmuBusyCycleEnabled | pmuBusyEvent0Enabled));
	static_assert(pmuBusyReasons(PmuRegisterSnapshot{.interruptEnabled = 0x80000001U}) == (pmuBusyCycleInterrupt | pmuBusyEvent0Interrupt));
	static_assert(pmuBusyReasons(PmuRegisterSnapshot{.overflow = 0x80000001U}) == (pmuBusyCycleOverflow | pmuBusyEvent0Overflow));
	static_assert(pmuBusyReasons(PmuRegisterSnapshot{.selector = 1U}) == 0U);
	static_assert(pmuBusyReasons(PmuRegisterSnapshot{.enabled = 2U}) == 0U); // Do not claim ownership of other event counters.
}
