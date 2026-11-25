#include <lexxos/composition/products/lyra_lcd_dma.h>

#include <lexxos/devices/rk3506/lcd_dma_controller.h>

namespace lexxos::composition::products {

platform::rk3506::LcdDmaController& lyraLcdDma() {
	static platform::rk3506::LcdDmaController controller;
	return controller;
}

platform::rk3506::LcdSpiDma& lyraLcdSpiDma() {
	return lyraLcdDma();
}

} // namespace lexxos::composition::products
