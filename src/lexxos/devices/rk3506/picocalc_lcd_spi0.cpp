// Compatibility entrypoints share the single product-owned SPI0 controller.
#include <lexxos/devices/rk3506/picocalc_lcd_spi0.h>

#include <lexxos/composition/products/lyra_lcd_spi.h>
#include <lexxos/devices/rk3506/lcd_spi_controller.h>

namespace lexxos::platform::rk3506 {

LcdSpi0Statistics lcdSpi0Statistics() {
	return composition::products::lyraLcdSpi().statistics();
}

bool lcdSpi0Initialize(std::uint32_t clockHz) {
	return composition::products::lyraLcdSpi().initialize(clockHz);
}

bool lcdSpi0SetClockHz(std::uint32_t clockHz) {
	return composition::products::lyraLcdSpi().setClockHz(clockHz);
}

std::uint32_t lcdSpi0ClockHz() {
	return composition::products::lyraLcdSpi().clockHz();
}

bool lcdSpi0Write(std::uint8_t const* data, std::size_t count) {
	return composition::products::lyraLcdSpi().write(data, count);
}

bool lcdSpi0SetRgb565Direct16(bool enabled) {
	return composition::products::lyraLcdSpi().setRgb565Direct16(enabled);
}

bool lcdSpi0Rgb565Direct16Enabled() {
	return composition::products::lyraLcdSpi().rgb565Direct16Enabled();
}

bool lcdSpi0WriteRgb565Words(std::uint16_t const* pixels, std::size_t count) {
	return composition::products::lyraLcdSpi().writeRgb565Words(pixels, count);
}

} // namespace lexxos::platform::rk3506
