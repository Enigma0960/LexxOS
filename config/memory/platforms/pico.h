#pragma once

#include <lexxos/runtime/runtime_profile.h>
#include <memory/pico_freertos_heap.h>

namespace lexxos::runtime::profiles {

constexpr void applyPicoPlatform(RuntimeProfile& profile) {
	profile.origin.platform = "pico-sdk";
	// Keep the eight-control v1 ABI and allow moderately sized v2 windows.
	profile.ui.maxResolvedControls = 12;
	profile.ui.maxTreeNodes = 16;
	profile.ui.maxListStores = 4;
	profile.graphics.maxImageHandles = 3;
	profile.graphics.maxImageDimension = 256;
	profile.graphics.maxImagePixels = 32768;
	profile.graphics.imageCacheBytes = 24 * 1024;
	profile.graphics.streamChunkBytes = 512;
	// FreeRTOS heap_4 backs task stacks, program arenas and queue allocations.
	// It is not a separate task stack pool; do not double-count it.
#if defined(LEXXOS_KERNEL_FREERTOS)
	profile.kernel.heapBytes = LEXXOS_PICO_FREERTOS_HEAP_BYTES;
#endif
	// Pico SDK builds without FreeRTOS do not own a heap_4 allocation.
}

} // namespace lexxos::runtime::profiles
