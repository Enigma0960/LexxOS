#include <limits>

#include <lexxos/kernel/synchronization.h>

#if defined(LEXXOS_PLATFORM_ESPIDF)
	#include <freertos/FreeRTOS.h>
	#include <freertos/semphr.h>
	#include <freertos/task.h>
#else
	#include <FreeRTOS.h>
	#include <semphr.h>
	#include <task.h>
#endif

#include <lexxos/kernel/freertos/detail/synchronization_support.h>

namespace lexxos::kernel {

using detail::waitTicks;

Mutex::Mutex() noexcept
    : _state(xSemaphoreCreateMutex()) {
}

Mutex::~Mutex() {
	if (_state) {
		vSemaphoreDelete(static_cast<SemaphoreHandle_t>(_state));
	}
}

bool Mutex::valid() const noexcept {
	return _state != nullptr;
}

bool Mutex::lock(std::uint32_t timeoutMs) noexcept {
	return _state && xSemaphoreTake(static_cast<SemaphoreHandle_t>(_state), waitTicks(timeoutMs)) == pdTRUE;
}

void Mutex::unlock() noexcept {
	if (_state) {
		(void) xSemaphoreGive(static_cast<SemaphoreHandle_t>(_state));
	}
}

} // namespace lexxos::kernel
