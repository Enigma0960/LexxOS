#pragma once
#include <lexxos/drivers/core/display/ili9488_transport.h>

namespace lexxos::drivers::core {

enum class Ili9488PowerMode {
	On,
	Off,
	Sleep,
	Invalid
};
bool setIli9488Power(Ili9488CommandTransport const& transport, Ili9488PowerMode previous, Ili9488PowerMode next);
bool setIli9488Window(Ili9488CommandTransport const& transport, std::uint16_t x0, std::uint16_t y0, std::uint16_t x1, std::uint16_t y1);
bool configureIli9488Scroll(Ili9488CommandTransport const& transport, int top, int area, int bottom);
bool setIli9488ScrollStart(Ili9488CommandTransport const& transport, int address);

} // namespace lexxos::drivers::core
