#pragma once
#include <cstddef>
#include <cstdint>
using StackType_t = std::uint32_t;
using TickType_t = std::uint32_t;
using BaseType_t = int;
using UBaseType_t = unsigned;
using TaskHandle_t = void*;
#define configSTACK_DEPTH_TYPE std::uint32_t
#define configMINIMAL_STACK_SIZE 1U
#define configMAX_PRIORITIES 8U
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_eTaskGetState 1

enum eTaskState {
	eRunning,
	eReady,
	eBlocked,
	eSuspended,
	eDeleted,
	eInvalid
};

#define pdPASS 1
#define pdFAIL 0
#define pdTRUE 1
#define pdFALSE 0
#define configTICK_RATE_HZ 1000U
#define portMAX_DELAY 0xFFFFFFFFU
#define pdMS_TO_TICKS(x) (x)
#define taskSCHEDULER_NOT_STARTED 0
#define taskSCHEDULER_RUNNING 1
int xTaskGetSchedulerState();
void vTaskSuspendAll();
int xTaskResumeAll();
void testYield();
#define taskYIELD() testYield()
std::size_t xPortGetFreeHeapSize();
std::size_t xPortGetMinimumEverFreeHeapSize();
