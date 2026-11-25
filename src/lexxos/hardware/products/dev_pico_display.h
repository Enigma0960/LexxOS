#pragma once

#include <lexxos/hardware/components/spi_tft.h>
#include <lexxos/hardware/products/dev_pico_interfaces.h>

namespace lexxos::hardware::products::dev_pico {

inline constexpr components::SpiTft display{
    {peripheralSpi.controller, {5}},
    {4},
    {6},
    {7},
    128, 160, false
};

} // namespace lexxos::hardware::products::dev_pico
