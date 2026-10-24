#pragma once

#include <cstdint>

#include <lexxos/platform/platform.h>
#include <lexxos/platform/stm32/stm32_gpio_types.h>

namespace lexxos::platform::stm32::detail {

[[nodiscard]] bool decodePin(GpioPin pin, DecodedPin& output);
[[nodiscard]] bool configureGpioInput(GpioPin pin, GpioPull pull);
[[nodiscard]] bool configureGpioOutput(GpioPin pin, bool initialValue);
[[nodiscard]] bool readGpio(GpioPin pin, bool& value);
[[nodiscard]] bool writeGpio(GpioPin pin, bool value);
[[nodiscard]] bool configureAlternateFunction(
    GpioPin pin,
    std::uint32_t alternateFunction,
    GpioPull pull,
    std::uint32_t speed);

} // namespace lexxos::platform::stm32::detail
