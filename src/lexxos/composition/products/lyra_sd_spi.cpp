#include <lexxos/composition/products/lyra_sd_spi.h>

#include <lexxos/devices/rk3506/sd_spi_controller.h>

namespace lexxos::composition::products {

platform::rk3506::SdSpiController& lyraSdSpi() {
	static platform::rk3506::SdSpiController controller;
	return controller;
}

} // namespace lexxos::composition::products
