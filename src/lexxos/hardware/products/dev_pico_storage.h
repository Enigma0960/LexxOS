#pragma once

#include <lexxos/hardware/components/spi_media.h>
#include <lexxos/hardware/products/dev_pico_interfaces.h>

namespace lexxos::hardware::products::dev_pico {

inline constexpr components::SpiSdCard sdCard{
    "sd0", {storageSpi.controller, {17}},
    {22},
    true
};

// LCD и PSRAM физически используют одну SPI1, но разные chip-select.
inline constexpr components::QuadSpiPsram psram{
    {peripheralSpi.controller, {9}},
    {13},
    {14}
};

} // namespace lexxos::hardware::products::dev_pico
