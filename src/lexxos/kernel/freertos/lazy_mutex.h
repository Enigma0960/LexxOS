#pragma once

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

namespace lexxos::kernel::freertos {

// Task context only. Allocate outside the critical section; publish once.
// Avoid exclusive CPU instructions: RK3506 bring-up runs with MMU disabled.
inline SemaphoreHandle_t ensureRecursiveMutex(void*& storage) {
	taskENTER_CRITICAL();
	auto mutex = static_cast<SemaphoreHandle_t>(storage);
	taskEXIT_CRITICAL();
	if (mutex != nullptr) {
		return mutex;
	}

	auto candidate = xSemaphoreCreateRecursiveMutex();
	taskENTER_CRITICAL();
	if (storage == nullptr) {
		storage = candidate;
		candidate = nullptr;
	}

	mutex = static_cast<SemaphoreHandle_t>(storage);
	taskEXIT_CRITICAL();
	if (candidate != nullptr) {
		vSemaphoreDelete(candidate);
	}

	return mutex;
}

} // namespace lexxos::kernel::freertos
