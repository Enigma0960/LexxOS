#pragma once

#include <array>

#include <lexxos/devices/display/ili9488_buffers.h>

namespace lexxos::composition::detail {

// Legacy-профиль размеров; память принадлежит одному display provider.
class Ili9488Workspace {
public:
	Ili9488Workspace() = default;
	Ili9488Workspace(Ili9488Workspace const&) = delete;
	Ili9488Workspace& operator=(Ili9488Workspace const&) = delete;
	devices::Ili9488Buffers buffers();

private:
#if defined(LEXXOS_PLATFORM_RK3506)
	static constexpr int repeatedPixelBurstCount = 4096;
#else
	static constexpr int repeatedPixelBurstCount = 1024;
#endif

#if !defined(LEXXOS_ILI9488_RGB888_BURST_PIXELS)
	#if defined(PICO_RP2040) && PICO_RP2040
	static constexpr int rgb888BurstPixels = 512;
	#elif defined(LEXXOS_PLATFORM_RK3506)
	static constexpr int rgb888BurstPixels = 16384;
	#else
	static constexpr int rgb888BurstPixels = 4096;
	#endif
#else
	static constexpr int rgb888BurstPixels = LEXXOS_ILI9488_RGB888_BURST_PIXELS;
#endif
	static_assert(rgb888BurstPixels > 0, "LEXXOS_ILI9488_RGB888_BURST_PIXELS must be positive");

	std::array<std::uint8_t, repeatedPixelBurstCount * 3> _repeated{};
	std::array<std::uint8_t, rgb888BurstPixels * 3> _pixels{};
};

} // namespace lexxos::composition::detail
