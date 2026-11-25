#pragma once

#include <lexxos/runtime/runtime_profile.h>

// SDK-owned heap, task slots, and stack pools are not reserved by this overlay.
// Leave kernel.* at zero until the backend has a single measured budget source.
namespace lexxos::runtime::profiles {

constexpr void applyEsp32S3Chip(RuntimeProfile& profile) {
	profile.origin.chip = "esp32s3";
}

} // namespace lexxos::runtime::profiles
