#pragma once

#include <cstdint>

#include <lexxos/drivers/core/display/ili9488_transport.h>

namespace lexxos::drivers::core {

enum class Ili9488PixelFormat : std::uint8_t {
	Rgb565 = 0x55,
	Rgb666 = 0x66,
};

using Ili9488InitTransport = Ili9488CommandTransport;

// Только протокол контроллера. Reset, владение шиной и очистка экрана — задача адаптера.
bool initializeIli9488(Ili9488InitTransport const& transport, Ili9488PixelFormat format, bool invertColors);

} // namespace lexxos::drivers::core
