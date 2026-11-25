#include <lexxos/composition/products/lyra_lcd_spi.h>

#include <lexxos/composition/products/lyra_lcd_dma.h>
#include <lexxos/devices/rk3506/lcd_spi_controller.h>

namespace lexxos::composition::products {

platform::rk3506::LcdSpiController& lyraLcdSpi() {
	static platform::rk3506::LcdSpiController controller{lyraLcdSpiDma()};
	return controller;
}

} // namespace lexxos::composition::products
