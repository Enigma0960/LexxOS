#include <lexxos/composition/products/lyra_sd_dma.h>

#include <lexxos/devices/rk3506/sd_dma_controller.h>

namespace lexxos::composition::products {

platform::rk3506::SdDmaController& lyraSdDma() {
	static platform::rk3506::SdDmaController controller;
	static bool const bound = platform::rk3506::sdDmaBindController(controller);
	(void) bound;
	return controller;
}

} // namespace lexxos::composition::products
