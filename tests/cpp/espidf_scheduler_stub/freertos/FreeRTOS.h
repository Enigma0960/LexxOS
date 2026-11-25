#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <mutex>

// Намеренно шире байта: проверяем контракт IDF независимо от текущего typedef порта.
using StackType_t = std::uint32_t;
using TickType_t = std::uint32_t;
using BaseType_t = int;
using UBaseType_t = unsigned;
using TaskHandle_t = void*;
using StaticSemaphore_t = std::mutex;
using SemaphoreHandle_t = StaticSemaphore_t*;
using portMUX_TYPE = std::mutex;
#define portMUX_INITIALIZER_UNLOCKED \
	{                                \
	}
#define configMINIMAL_STACK_SIZE 1024U
#define configMAX_PRIORITIES 512U
#define portMAX_DELAY 0xFFFFFFFFU
#define pdPASS 1
#define pdFAIL 0
#define pdTRUE 1
#define pdMS_TO_TICKS(x) (x)
#define configASSERT(x)   \
	do {                  \
		if (!(x))         \
			std::abort(); \
	} while (false)
void testYield();
#define taskYIELD() testYield()
void testEnterCritical(portMUX_TYPE* mux, bool isr);
void testExitCritical(portMUX_TYPE* mux, bool isr);
#define taskENTER_CRITICAL(mux) testEnterCritical(mux, false)
#define taskEXIT_CRITICAL(mux) testExitCritical(mux, false)
#define taskENTER_CRITICAL_ISR(mux) testEnterCritical(mux, true)
#define taskEXIT_CRITICAL_ISR(mux) testExitCritical(mux, true)
std::size_t xPortGetFreeHeapSize();
std::size_t xPortGetMinimumEverFreeHeapSize();
