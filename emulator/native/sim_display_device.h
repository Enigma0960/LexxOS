#pragma once

#include <string_view>

#include <lexxos/devices/display_device.h>

namespace lexxos::emulator {

class SimDisplayDevice final : public lexxos::devices::DisplayDevice {
public:
	[[nodiscard]] std::string_view name() const override;
	[[nodiscard]] bool isReady() const override;

	[[nodiscard]] int width() const override;
	[[nodiscard]] int height() const override;
	[[nodiscard]] lexxos::devices::PixelFormat nativeFormat() const override;

	bool clear(std::uint16_t rgb565) override;
	bool beginFrame() override;
	bool blitRgb565(lexxos::devices::Rect const& rect, std::uint16_t const* pixels, int stridePixels) override;
	bool endFrame() override;
};

[[nodiscard]] SimDisplayDevice& simDisplayDevice();

} // namespace lexxos::emulator
