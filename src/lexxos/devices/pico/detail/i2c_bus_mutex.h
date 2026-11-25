#pragma once

#include <atomic>
#include <cstdint>

#if defined(LEXXOS_KERNEL_FREERTOS)
	#include <FreeRTOS.h>
	#include <semphr.h>
#endif

namespace lexxos::devices {

class I2cBusMutex {
public:
	[[nodiscard]] bool initialize();

	[[nodiscard]] bool lock();

	void unlock();

private:
#if defined(LEXXOS_KERNEL_FREERTOS)
	SemaphoreHandle_t _mutex{nullptr};
#else
	std::atomic_flag _locked = ATOMIC_FLAG_INIT;
	std::uint32_t _depth{0};
#endif
};

} // namespace lexxos::devices
