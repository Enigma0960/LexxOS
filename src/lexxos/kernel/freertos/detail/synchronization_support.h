#pragma once

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

namespace lexxos::kernel::detail {

TickType_t waitTicks(std::uint32_t milliseconds);

} // namespace lexxos::kernel::detail
