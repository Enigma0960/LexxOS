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

Semaphore::Semaphore(std::uint32_t maximum, std::uint32_t initial) noexcept {
	if (maximum != 0U && initial <= maximum && maximum <= std::numeric_limits<UBaseType_t>::max()) {
		_state = xSemaphoreCreateCounting(static_cast<UBaseType_t>(maximum), static_cast<UBaseType_t>(initial));
	}
}

Semaphore::~Semaphore() {
	if (_state) {
		vSemaphoreDelete(static_cast<SemaphoreHandle_t>(_state));
	}
}

bool Semaphore::valid() const noexcept {
	return _state != nullptr;
}

bool Semaphore::acquire(std::uint32_t timeoutMs) noexcept {
	return _state && xSemaphoreTake(static_cast<SemaphoreHandle_t>(_state), waitTicks(timeoutMs)) == pdTRUE;
}

bool Semaphore::release() noexcept {
	return _state && xSemaphoreGive(static_cast<SemaphoreHandle_t>(_state)) == pdTRUE;
}

} // namespace lexxos::kernel
