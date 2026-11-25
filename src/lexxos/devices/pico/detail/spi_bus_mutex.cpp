#include <lexxos/devices/pico/detail/spi_bus_mutex.h>

namespace lexxos::devices {

bool SpiBusMutex::initialize() {
#if defined(LEXXOS_KERNEL_FREERTOS)
	if (_mutex == nullptr) {
		_mutex = xSemaphoreCreateRecursiveMutex();
	}

	return _mutex != nullptr;
#else
	return true;
#endif
}

bool SpiBusMutex::lock() {
#if defined(LEXXOS_KERNEL_FREERTOS)
	if (_mutex == nullptr) {
		return false;
	}

	return xSemaphoreTakeRecursive(_mutex, portMAX_DELAY) == pdTRUE;
#else
	if (_depth == 0) {
		while (_locked.test_and_set(std::memory_order_acquire)) {
		}
	}

	++_depth;
	return true;
#endif
}

void SpiBusMutex::unlock() {
#if defined(LEXXOS_KERNEL_FREERTOS)
	if (_mutex != nullptr) {
		(void) xSemaphoreGiveRecursive(_mutex);
	}
#else
	if (_depth == 0) {
		return;
	}

	--_depth;
	if (_depth == 0) {
		_locked.clear(std::memory_order_release);
	}
#endif
}

} // namespace lexxos::devices
