#pragma once

#include <lexxos/platform/platform.h>

namespace lexxos::platform::pico::detail {

bool configureGpioInterrupt(
    lexxos::platform::GpioPin pin,
    lexxos::platform::GpioInterruptEdge edge,
    lexxos::platform::GpioInterruptHandler callback,
    void* userData);
void disableGpioInterrupt(lexxos::platform::GpioPin pin);

} // namespace lexxos::platform::pico::detail
