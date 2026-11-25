#pragma once

#include <array>
#include <cstdint>

#include <lexxos/hardware/products/lyra_policy.h>

namespace lexxos::hardware::products {

// Boot SDMMC stays read-only. Only the removable PicoCalc socket is writable.
inline constexpr std::array picoCalcLyraStoragePolicy{
    lyraSystemStoragePolicy,
    StoragePolicy{
                  .deviceName = "sd1",
                  .mountPath = "/mnt/sd",
                  .role = StorageRole::Removable,
                  .hotSwappable = true,
                  .autoMount = true,
                  .readOnlyByDefault = false,
                  },
};

// On the cooperative DMA profile redraw through the framebuffers is faster.
// FreeRTOS currently uses SPI0 PIO: full-screen software scrolling is much
// more expensive, so use the ILI9488 vertical scroll controller instead.
#if defined(LEXXOS_RK3506_FREERTOS_PERIPHERALS)
inline constexpr bool picoCalcLyraHardwareScrollEnabled = true;
#else
inline constexpr bool picoCalcLyraHardwareScrollEnabled = false;
#endif

inline constexpr std::uint32_t picoCalcLyraDefaultDisplayHz = 23437500;

} // namespace lexxos::hardware::products
