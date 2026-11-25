#pragma once

#include <array>

#include <lexxos/hardware/products/lyra_product.h>
#include <lexxos/hardware/products/picocalc_lyra_policy.h>

namespace lexxos::hardware::products {

inline constexpr std::array lyraStandaloneStoragePolicy{
    lyraSystemStoragePolicy,
};

inline constexpr LyraProduct lyraStandaloneProduct{
    .id = "lyra_standalone",
    .name = "Luckfox Lyra",
    .carrier = LyraCarrier::None,
    .picoCalcWiring = nullptr,
    .storage = lyraStandaloneStoragePolicy,
};
inline constexpr LyraProduct picoCalcLyraProduct{
    .id = "picocalc_lyra",
    .name = "PicoCalc / Luckfox Lyra",
    .carrier = LyraCarrier::PicoCalc,
    .picoCalcWiring = &picoCalcLyraWiring,
    .storage = picoCalcLyraStoragePolicy,
};

static_assert(validateLyraProduct(lyraStandaloneProduct) == LyraProductError::None);
static_assert(validateLyraProduct(picoCalcLyraProduct) == LyraProductError::None);

} // namespace lexxos::hardware::products
