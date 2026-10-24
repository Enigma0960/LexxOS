#pragma once

#if defined(LEXXOS_PLATFORM_ESPIDF)
	#include <freertos/FreeRTOS.h>
	#include <freertos/task.h>
#else
	#include <FreeRTOS.h>
	#include <task.h>
#endif

#include <lexxos/kernel/scheduler.h>

namespace lexxos::kernel::freertos::detail {

struct FreeRtosTask {
	TaskEntry entry{nullptr};
	void* userData{nullptr};
	TaskHandle_t handle{nullptr};
	TaskSnapshot snapshot{};
	bool allocated{false};
};

} // namespace lexxos::kernel::freertos::detail
