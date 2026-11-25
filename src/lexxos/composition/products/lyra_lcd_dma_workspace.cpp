#include <lexxos/composition/products/lyra_lcd_dma_workspace.h>

#include <lexxos/devices/rk3506/lcd_dma_controller.h>

namespace lexxos::composition::products {

devices::Ili9488Buffers LyraLcdDmaWorkspace::buffers() {
	return {_pixels, _repeated};
}

bool LyraLcdDmaWorkspace::registerDmaSource(platform::rk3506::LcdDmaController& controller) {
	return controller.registerSource(_pixels.data(), _pixels.size());
}

} // namespace lexxos::composition::products
