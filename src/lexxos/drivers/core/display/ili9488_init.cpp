#include <lexxos/drivers/core/display/ili9488_init.h>

namespace lexxos::drivers::core {

bool initializeIli9488(Ili9488InitTransport const& transport, Ili9488PixelFormat format, bool invertColors) {
	if (transport.write == nullptr || transport.sleepMs == nullptr || (format != Ili9488PixelFormat::Rgb565 && format != Ili9488PixelFormat::Rgb666)) {
		return false;
	}

	struct Step {
		std::uint8_t command;
		std::uint8_t size;
		std::uint8_t data[15];
		std::uint32_t delayMs;
	};

	// Последовательность legacy-драйвера сохранена, включая повторный MADCTL.
	Step const steps[]{
	    {                                                 0xE0, 15, {0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78, 0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F},   0},
	    {                                                 0xE1, 15, {0x00, 0x16, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45, 0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37},   0},
	    {                                                 0xC0,  2,                                                                               {0x17, 0x15},   0},
	    {                                                 0xC1,  1,                                                                                     {0x41},   0},
	    {                                                 0xC5,  3,                                                                         {0x00, 0x12, 0x80},   0},
	    {                                                 0x36,  1,                                                                                     {0x48},   0},
	    {                                                 0x3A,  1,                                                        {static_cast<std::uint8_t>(format)},   0},
	    {                                                 0xB0,  1,                                                                                     {0x00},   0},
	    {                                                 0xB1,  1,                                                                                     {0xA0},   0},
	    {static_cast<std::uint8_t>(invertColors ? 0x21 : 0x20),  0,                                                                                         {},   0},
	    {                                                 0xB4,  1,                                                                                     {0x02},   0},
	    {                                                 0xB6,  3,                                                                         {0x02, 0x02, 0x3B},   0},
	    {                                                 0xB7,  1,                                                                                     {0xC6},   0},
	    {                                                 0xE9,  1,                                                                                     {0x00},   0},
	    {                                                 0xF7,  4,                                                                   {0xA9, 0x51, 0x2C, 0x82},   0},
	    {                                                 0x11,  0,                                                                                         {}, 120},
	    {                                                 0x29,  0,                                                                                         {}, 120},
	    {                                                 0x36,  1,                                                                                     {0x48},   0},
	};
	for (auto const& step : steps) {
		if (!transport.write(transport.context, step.command, step.size ? step.data : nullptr, step.size)) {
			return false;
		}

		if (step.delayMs != 0) {
			transport.sleepMs(transport.context, step.delayMs);
		}
	}

	return true;
}

} // namespace lexxos::drivers::core
