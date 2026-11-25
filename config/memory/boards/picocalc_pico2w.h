#pragma once

#include <lexxos/runtime/runtime_profile.h>

namespace lexxos::runtime::profiles {

constexpr void applyPicoCalcPico2wBoard(RuntimeProfile& profile) {
	// The RP2350 PicoCalc renderer needs the larger console mirror stack.
	profile.taskStacks.consoleMirrorTaskBytes = 8 * 1024;
	// RP2350 PicoCalc retains the full v2 resolved-control capacity.
	profile.ui.maxResolvedControls = 16;
	profile.ui.maxTreeNodes = 16;
}

} // namespace lexxos::runtime::profiles
