#pragma once

#include <lexxos/hardware/components/pcf8575_input.h>
#include <lexxos/hardware/products/dev_pico_interfaces.h>

namespace lexxos::hardware::products::dev_pico {

inline constexpr components::Pcf8575 keyboardExpander{
    "kbd-pcf8575", debugI2c.controller, 0x20, {27}};

inline constexpr components::Pcf8575Matrix keyboard{
    "main-keyboard", keyboardExpander.name, {8, 9, 10, 11, 12, 13},
    {0, 1, 2, 3}
};

inline constexpr components::SegaGamepad segaGamepad{
    "sega-gamepad", keyboardExpander.name, 4, 5, 6, 7, 14, 15, {2}};

} // namespace lexxos::hardware::products::dev_pico
