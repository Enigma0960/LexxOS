#pragma once

#include <lexxos/platform/platform.h>

namespace lexxos::platform::pico::detail {

struct GpioInterruptRegistration {
	bool active{false};
	GpioPin pin{};
	GpioInterruptHandler handler{nullptr};
	void* userData{nullptr};
};

} // namespace lexxos::platform::pico::detail
