#include <lexxos/runtime/selected_runtime_profile.h>

#if defined(LEXXOS_MEMORY_PLATFORM_PICO) && defined(LEXXOS_KERNEL_FREERTOS)
	#include <FreeRTOSConfig.h>
#endif

using lexxos::runtime::standardRuntimeProfile;

static_assert(lexxos::runtime::validateRuntimeProfile(standardRuntimeProfile));

#if defined(LEXXOS_MEMORY_PLATFORM_PICO) && defined(LEXXOS_KERNEL_FREERTOS)
static_assert(standardRuntimeProfile.kernel.heapBytes == LEXXOS_PICO_FREERTOS_HEAP_BYTES);
static_assert(standardRuntimeProfile.kernel.heapBytes == configTOTAL_HEAP_SIZE);
	#if defined(LEXXOS_PICO_FREERTOS_HEAP_KB) && LEXXOS_PICO_FREERTOS_HEAP_KB > 0
static_assert(standardRuntimeProfile.kernel.heapBytes == LEXXOS_PICO_FREERTOS_HEAP_KB * 1024U);
	#elif defined(LEXXOS_MEMORY_CHIP_RP2350)
static_assert(standardRuntimeProfile.kernel.heapBytes == 224U * 1024U);
	#else
static_assert(standardRuntimeProfile.kernel.heapBytes == 126U * 1024U);
	#endif
#else
static_assert(standardRuntimeProfile.kernel.heapBytes == 0);
#endif

static_assert(standardRuntimeProfile.kernel.taskStackPoolBytes == 0);
static_assert(standardRuntimeProfile.kernel.taskSlots == 0);
static_assert(standardRuntimeProfile.shell.maxLineLength == 2048);
static_assert(standardRuntimeProfile.events.lifecycleQueueCapacity == 16);
static_assert(standardRuntimeProfile.programs.maxProcesses == 4);

#if defined(LEXXOS_MEMORY_PLATFORM_PICO)
	#if defined(LEXXOS_MEMORY_BOARD_PICOCALC_PICO2W)
static_assert(standardRuntimeProfile.ui.maxResolvedControls == 16);
static_assert(standardRuntimeProfile.ui.maxTreeNodes == 16);
	#else
static_assert(standardRuntimeProfile.ui.maxResolvedControls == 12);
static_assert(standardRuntimeProfile.ui.maxTreeNodes == 16);
	#endif
#else
static_assert(standardRuntimeProfile.ui.maxResolvedControls == 32);
static_assert(standardRuntimeProfile.ui.maxTreeNodes == 32);
#endif

#if defined(LEXXOS_MEMORY_CHIP_RP2040)
static_assert(standardRuntimeProfile.origin.chip[0] == 'r');
static_assert(standardRuntimeProfile.origin.chip[2] == '2');
static_assert(standardRuntimeProfile.taskStacks.consoleMirrorTaskBytes == 4096);
#elif defined(LEXXOS_MEMORY_CHIP_RP2350)
static_assert(standardRuntimeProfile.origin.chip[3] == '3');
	#if defined(LEXXOS_MEMORY_BOARD_PICOCALC_PICO2W)
static_assert(standardRuntimeProfile.taskStacks.consoleMirrorTaskBytes == 8192);
	#else
static_assert(standardRuntimeProfile.taskStacks.consoleMirrorTaskBytes == 4096);
	#endif
#elif defined(LEXXOS_MEMORY_CHIP_ESP32) || defined(LEXXOS_MEMORY_CHIP_ESP32S3) || defined(LEXXOS_MEMORY_CHIP_ESP32P4)
static_assert(standardRuntimeProfile.origin.chip[0] == 'e');
	#if defined(LEXXOS_MEMORY_BOARD_ESPIDF_LCD)
static_assert(standardRuntimeProfile.taskStacks.consoleMirrorTaskBytes == 8192);
	#else
static_assert(standardRuntimeProfile.taskStacks.consoleMirrorTaskBytes == 4096);
	#endif
#else
	#error No SDK memory chip selected for test
#endif

int main() {
	return 0;
}
