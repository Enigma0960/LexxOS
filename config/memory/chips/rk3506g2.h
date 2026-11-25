#pragma once

#include <lexxos/runtime/runtime_profile.h>

// RK3506G2 CPU0 DDR-backed LexxOS *software* budgets, not the physical DDR
// layout. DDR initialization and reservations remain owned by the loader.
namespace lexxos::runtime::profiles {

constexpr void applyRk3506g2Chip(RuntimeProfile& profile) {
	profile.origin.chip = "rk3506g2";
	// Preserve the measured bring-up allocator capacities for the first move.
	profile.kernel.heapBytes = 256 * 1024;
	profile.kernel.taskStackPoolBytes = 96 * 1024;
}

} // namespace lexxos::runtime::profiles
