#pragma once

#include <atomic>

#if defined(LEXXOS_KERNEL_FREERTOS)
	#include <FreeRTOS.h>
	#include <semphr.h>
#endif

namespace lexxos::devices::storage::detail {

class SdCardTransferMutex {
public:
	SdCardTransferMutex(SdCardTransferMutex const&) = delete;
	SdCardTransferMutex& operator=(SdCardTransferMutex const&) = delete;

	static SdCardTransferMutex& instance();

	[[nodiscard]] bool lock();
	void unlock();

private:
	SdCardTransferMutex() = default;

#if defined(LEXXOS_KERNEL_FREERTOS)
	[[nodiscard]] bool ensureCreated();

	SemaphoreHandle_t _mutex{nullptr};
#else
	std::atomic_flag _locked = ATOMIC_FLAG_INIT;
#endif
};

} // namespace lexxos::devices::storage::detail
