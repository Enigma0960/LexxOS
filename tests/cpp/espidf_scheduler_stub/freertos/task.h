#pragma once
#include <freertos/FreeRTOS.h>

enum eTaskState {
	eRunning,
	eReady,
	eBlocked,
	eSuspended,
	eDeleted,
	eInvalid
};

BaseType_t xTaskCreate(void (*entry)(void*), char const* name, std::uint32_t bytes, void* argument,
    UBaseType_t priority, TaskHandle_t* handle);
BaseType_t xTaskCreatePinnedToCore(void (*entry)(void*), char const* name, std::uint32_t bytes, void* argument,
    UBaseType_t priority, TaskHandle_t* handle, BaseType_t core);
void vTaskDelete(TaskHandle_t handle);
void vTaskSuspend(TaskHandle_t handle);
void vTaskDelay(TickType_t ticks);
eTaskState eTaskGetState(TaskHandle_t handle);
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t handle);
