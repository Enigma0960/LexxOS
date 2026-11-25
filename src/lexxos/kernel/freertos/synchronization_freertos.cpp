#include <limits>

#include <lexxos/kernel/freertos/detail/synchronization_support.h>
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

namespace lexxos::kernel::detail {

TickType_t waitTicks(std::uint32_t milliseconds) {
	if (milliseconds == lexxos::kernel::waitForever) {
		return portMAX_DELAY;
	}

	if (milliseconds == 0U) {
		return 0;
	}

	// pdMS_TO_TICKS may overflow before the conversion; round up so a
	// non-zero request never accidentally becomes an immediate poll.
	auto const ticks = (static_cast<std::uint64_t>(milliseconds) * configTICK_RATE_HZ + 999U) / 1000U;
	auto const maximum = static_cast<std::uint64_t>(portMAX_DELAY) - 1U;
	return static_cast<TickType_t>(ticks > maximum ? maximum : ticks);
}

} // namespace lexxos::kernel::detail

namespace lexxos::kernel {

void synchronizationBackoff() noexcept {
	// A higher-priority waiter must not starve the task completing its wakeup.
	vTaskDelay(1);
}

} // namespace lexxos::kernel
