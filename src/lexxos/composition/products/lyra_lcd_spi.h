#pragma once

namespace lexxos::platform::rk3506 {

class LcdSpiController;

} // namespace lexxos::platform::rk3506

namespace lexxos::composition::products {

// Boot-lifetime owner shared by the panel backend and diagnostic aliases.
platform::rk3506::LcdSpiController& lyraLcdSpi();

} // namespace lexxos::composition::products
