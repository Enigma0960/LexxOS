#pragma once

// Shared C/C++ values: FreeRTOSConfig.h and RuntimeProfile must resolve the
// same heap_4 capacity. These are allocator budgets, not total chip SRAM.
#define LEXXOS_PICO_FREERTOS_HEAP_RP2040_BYTES (126U * 1024U)
#define LEXXOS_PICO_FREERTOS_HEAP_RP2350_BYTES (224U * 1024U)

// CMake's effective override also carries the 88-KiB first-generation
// PicoCalc product cap and explicit per-preset heap sizes.
#if defined(LEXXOS_PICO_FREERTOS_HEAP_KB) && LEXXOS_PICO_FREERTOS_HEAP_KB > 0
	#define LEXXOS_PICO_FREERTOS_HEAP_BYTES ((LEXXOS_PICO_FREERTOS_HEAP_KB) * 1024U)
#elif (defined(PICO_RP2350) && PICO_RP2350) || defined(LEXXOS_MEMORY_CHIP_RP2350)
	#define LEXXOS_PICO_FREERTOS_HEAP_BYTES LEXXOS_PICO_FREERTOS_HEAP_RP2350_BYTES
#else
	#define LEXXOS_PICO_FREERTOS_HEAP_BYTES LEXXOS_PICO_FREERTOS_HEAP_RP2040_BYTES
#endif
