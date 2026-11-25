#pragma once

namespace lexxos::platform::rk3506 {

class SdDmaController;

} // namespace lexxos::platform::rk3506

namespace lexxos::composition::products {

platform::rk3506::SdDmaController& lyraSdDma();

} // namespace lexxos::composition::products
