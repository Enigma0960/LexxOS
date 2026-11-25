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

Event::Event() noexcept
    : _state(xSemaphoreCreateBinary()) {
}

Event::~Event() {
	if (_state) {
		vSemaphoreDelete(static_cast<SemaphoreHandle_t>(_state));
	}
}

bool Event::valid() const noexcept {
	return _state != nullptr;
}

bool Event::wait(std::uint32_t timeoutMs) noexcept {
	return _state && xSemaphoreTake(static_cast<SemaphoreHandle_t>(_state), waitTicks(timeoutMs)) == pdTRUE;
}

bool Event::signal() noexcept {
	return _state && xSemaphoreGive(static_cast<SemaphoreHandle_t>(_state)) == pdTRUE;
}

bool Event::signalFromIsr(bool& higherPriorityTaskWoken) noexcept {
	if (!_state) {
		return false;
	}

	BaseType_t woken = pdFALSE;
	auto const ok = xSemaphoreGiveFromISR(static_cast<SemaphoreHandle_t>(_state), &woken) == pdTRUE;
	higherPriorityTaskWoken = higherPriorityTaskWoken || woken == pdTRUE;
	return ok;
}

} // namespace lexxos::kernel
