#pragma once

#include <lexxos/runtime/runtime_profile.h>

namespace lexxos::runtime::profiles {

constexpr void applyEspIdfLcdBoard(RuntimeProfile& profile) {
	// Cardputer and JC ESP32-P4 use the LCD console mirror.
	profile.taskStacks.consoleMirrorTaskBytes = 8 * 1024;
}

} // namespace lexxos::runtime::profiles
