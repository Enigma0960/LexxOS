#pragma once

#include <array>

#include <lexxos/devices/display/ili9488_buffers.h>

namespace lexxos::platform::rk3506 {

class LcdDmaController;

} // namespace lexxos::platform::rk3506

namespace lexxos::composition::products {

// Статическая DDR под целую wire-область 320x320 RGB666. Владелец — продукт.
class LyraLcdDmaWorkspace {
public:
	LyraLcdDmaWorkspace() = default;
	LyraLcdDmaWorkspace(LyraLcdDmaWorkspace const&) = delete;
	LyraLcdDmaWorkspace& operator=(LyraLcdDmaWorkspace const&) = delete;
	devices::Ili9488Buffers buffers();
	bool registerDmaSource(platform::rk3506::LcdDmaController& controller);

private:
	alignas(64) std::array<std::uint8_t, 320 * 320 * 3> _pixels{};
	alignas(64) std::array<std::uint8_t, 4096 * 3> _repeated{};
};

} // namespace lexxos::composition::products
