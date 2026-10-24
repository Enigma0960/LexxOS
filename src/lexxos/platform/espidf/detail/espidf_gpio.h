#pragma once

#include <lexxos/platform/platform.h>

namespace lexxos::platform::espidf::detail {

struct GpioInterruptSlot {
	lexxos::platform::GpioInterruptHandler handler{nullptr};
	void* userData{nullptr};
};

void initializeStatusLed();

} // namespace lexxos::platform::espidf::detail
