#pragma once

namespace lexxos::platform::rk3506 {

class LcdDmaController;
class LcdSpiDma;

} // namespace lexxos::platform::rk3506

namespace lexxos::composition::products {

// One boot-lifetime DMA owner for both the SPI dependency and IRQ binding.
platform::rk3506::LcdDmaController& lyraLcdDma();
platform::rk3506::LcdSpiDma& lyraLcdSpiDma();

} // namespace lexxos::composition::products
