#pragma once

namespace lexxos::platform::rk3506 {

class SdSpiController;

} // namespace lexxos::platform::rk3506

namespace lexxos::composition::products {

// Boot-lifetime owner shared by SD protocol and diagnostic compatibility calls.
platform::rk3506::SdSpiController& lyraSdSpi();

} // namespace lexxos::composition::products
