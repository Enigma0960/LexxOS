#pragma once

#include <lexxos/hardware/products/lyra_products.h>

namespace lexxos::composition::products {

// Current build supports PicoCalc + Lyra. A standalone build still requires
// migration of remaining hardware initialization and build options; this is
// not a runtime switch. Device and storage runtime dispatch use this selection.
inline constexpr auto const& selectedLyraProduct = hardware::products::picoCalcLyraProduct;

} // namespace lexxos::composition::products
