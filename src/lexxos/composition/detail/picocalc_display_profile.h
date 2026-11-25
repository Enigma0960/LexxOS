#pragma once

#include <lexxos/devices/display/ili9488_panel.h>

namespace lexxos::composition::detail {

// Совместимость с прежним флагом сборки ограничена composition.
#if defined(LEXXOS_ILI9488_RGB565_WIRE) && LEXXOS_ILI9488_RGB565_WIRE
inline constexpr auto picoCalcDisplayWireFormat = drivers::core::Ili9488PixelFormat::Rgb565;
#else
inline constexpr auto picoCalcDisplayWireFormat = drivers::core::Ili9488PixelFormat::Rgb666;
#endif

inline constexpr devices::Ili9488ResetPolicy picoCalcDisplayReset{
    .enabled = true,
    .activeLow = true,
    .beforeAssertMs = 10,
    .assertedMs = 10,
    .recoveryMs = 200,
};

} // namespace lexxos::composition::detail
