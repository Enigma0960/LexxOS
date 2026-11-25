#pragma once

#include <lexxos/composition/products/lyra_device_policy.h>
#include <lexxos/composition/products/selected_lyra_product.h>
#include <lexxos/feature.h>

namespace lexxos::composition::products {

inline constexpr auto selectedLyraResolvedEndpoints = hardware::products::resolveLyraEndpoints(selectedLyraProduct);
static_assert(selectedLyraResolvedEndpoints.has_value());

#if defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
inline constexpr bool selectedLyraJcsSouthbridge = true;
#else
inline constexpr bool selectedLyraJcsSouthbridge = false;
#endif
inline constexpr auto selectedLyraEndpoints = applyLyraDevicePolicy(*selectedLyraResolvedEndpoints, selectedLyraJcsSouthbridge);
inline constexpr bool selectedLyraDisplayEnabled = FEATURE_ENABLED(DISPLAY);
#if defined(LEXXOS_RK3506_LCD_SPI_HZ)
inline constexpr std::uint32_t selectedLyraDisplayHz = LEXXOS_RK3506_LCD_SPI_HZ;
#else
inline constexpr std::uint32_t selectedLyraDisplayHz = hardware::products::picoCalcLyraDefaultDisplayHz;
#endif

} // namespace lexxos::composition::products
