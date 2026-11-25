#pragma once
#include <FreeRTOS.h>
BaseType_t xTaskCreate(void (*entry)(void*), char const*, std::uint32_t depth, void* argument, UBaseType_t, TaskHandle_t*);
void vTaskDelete(TaskHandle_t);
void vTaskSuspend(TaskHandle_t);
eTaskState eTaskGetState(TaskHandle_t);
void vTaskDelay(TickType_t);
void vTaskStartScheduler();
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t);
void testEnterCritical();
void testExitCritical();
#define taskENTER_CRITICAL() testEnterCritical()
#define taskEXIT_CRITICAL() testExitCritical()
