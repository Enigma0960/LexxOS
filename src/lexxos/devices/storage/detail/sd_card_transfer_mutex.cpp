#include <lexxos/devices/storage/detail/sd_card_transfer_mutex.h>

namespace lexxos::devices::storage::detail {

SdCardTransferMutex& SdCardTransferMutex::instance() {
	static SdCardTransferMutex mutex;
	return mutex;
}

bool SdCardTransferMutex::lock() {
#if defined(LEXXOS_KERNEL_FREERTOS)
	if (!ensureCreated()) {
		return false;
	}
	return xSemaphoreTakeRecursive(_mutex, portMAX_DELAY) == pdTRUE;
#else
	while (_locked.test_and_set(std::memory_order_acquire)) {
	}
	return true;
#endif
}

void SdCardTransferMutex::unlock() {
#if defined(LEXXOS_KERNEL_FREERTOS)
	if (_mutex != nullptr) {
		(void) xSemaphoreGiveRecursive(_mutex);
	}
#else
	_locked.clear(std::memory_order_release);
#endif
}

#if defined(LEXXOS_KERNEL_FREERTOS)
bool SdCardTransferMutex::ensureCreated() {
	if (_mutex == nullptr) {
		_mutex = xSemaphoreCreateRecursiveMutex();
	}
	return _mutex != nullptr;
}
#endif

} // namespace lexxos::devices::storage::detail
