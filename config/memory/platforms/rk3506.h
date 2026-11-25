#pragma once

#include <lexxos/runtime/runtime_profile.h>

// Limits of the current cooperative CPU0 scheduler implementation.
// A slot is reserved for the bootstrap/idle context.
namespace lexxos::runtime::profiles {

constexpr void applyRk3506Platform(RuntimeProfile& profile) {
	profile.origin.platform = "rk3506";
	profile.kernel.taskSlots = 8;
	profile.kernel.minimumTaskStackBytes = 512;
	profile.kernel.maximumTaskStackBytes = 32 * 1024;
	// This was previously an RK3506-specific branch in runtime_tasks.cpp.
	profile.taskStacks.consoleMirrorTaskBytes = 8 * 1024;
}

} // namespace lexxos::runtime::profiles
