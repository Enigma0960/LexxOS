#include <FreeRTOS.h>
#include <stdint.h>

#if defined(LEXXOS_PLATFORM_STM32F4) && (configAPPLICATION_ALLOCATED_HEAP == 1)
uint8_t ucHeap[configTOTAL_HEAP_SIZE] __attribute__((section(".ccmram"), aligned(8)));
#endif
