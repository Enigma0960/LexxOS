#include <cassert>

#include <lexxos/platform/rockchip/rk3506/pmu_profile.h>

using lexxos::platform::rk3506::pmuBusyGlobalEnable;
using lexxos::platform::rk3506::pmuBusyReasons;
using lexxos::platform::rk3506::pmuDifference;
using lexxos::platform::rk3506::pmuOwnershipBlocked;
using lexxos::platform::rk3506::PmuRegisterSnapshot;
using lexxos::platform::rk3506::PmuSample;

int main() {
	// Firmware leaves PMCR.E set on Lyra; all counters/IRQs/overflow are zero.
	constexpr PmuRegisterSnapshot inherited{.pmcr = 0x41072039U, .valid = true};
	static_assert(pmuBusyReasons(inherited) == pmuBusyGlobalEnable);
	static_assert(!pmuOwnershipBlocked(inherited));
	static_assert(!pmuOwnershipBlocked(PmuRegisterSnapshot{.valid = true}));
	static_assert(pmuOwnershipBlocked(PmuRegisterSnapshot{}));
	static_assert(pmuOwnershipBlocked(PmuRegisterSnapshot{.pmcr = 0x41072039U, .enabled = 1U, .valid = true}));
	static_assert(pmuOwnershipBlocked(PmuRegisterSnapshot{.pmcr = 0x41072039U, .enabled = 1U << 31U, .valid = true}));
	static_assert(pmuOwnershipBlocked(PmuRegisterSnapshot{.pmcr = 0x41072039U, .interruptEnabled = 1U, .valid = true}));
	static_assert(pmuOwnershipBlocked(PmuRegisterSnapshot{.pmcr = 0x41072039U, .overflow = 1U << 31U, .valid = true}));

	constexpr auto valid = pmuDifference({true, 10U, 22U, 0U}, {true, 110U, 222U, 0U});
	static_assert(valid.valid && valid.cycles == 100U && valid.instructions == 200U);
	constexpr auto wrap = pmuDifference({true, 0xFFFFFFFEU, 0xFFFFFFFCU, 0U}, {true, 3U, 2U, 0U});
	static_assert(wrap.valid && wrap.cycles == 5U && wrap.instructions == 6U);
	static_assert(!pmuDifference({}, {true, 100U, 1U, 0U}).valid);
	static_assert(!pmuDifference({true, 1U, 1U, 0U}, {true, 2U, 2U, 1U}).valid);
	static_assert(!pmuDifference({true, 1U, 1U, 1U}, {true, 2U, 2U, 0U}).valid);
	assert(valid.instructions == 200U);
}
