#pragma once

#include <lexxos/runtime/runtime_profile.h>

namespace lexxos::runtime::profiles {

constexpr void applyEspIdfPlatform(RuntimeProfile& profile) {
	profile.origin.platform = "esp-idf";
	// Вложенные вызовы FatFs/SPI и инициализация Wi-Fi требуют большего стека.
	profile.programs.filesystemMutationStackBytes = 8192;
	profile.programs.networkStatusStackBytes = 8192;
	// The allocator and task slots are owned by ESP-IDF, not this profile.
	// A larger LCD console stack is selected only for LCD-console boards.
}

} // namespace lexxos::runtime::profiles
