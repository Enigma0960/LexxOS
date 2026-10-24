#pragma once

#include <cstdint>
#include <string_view>

#include <lexxos/devices/display_device.h>
#include <lexxos/platform/board.h>

namespace lexxos::platform {
struct PlatformServices;
} // namespace lexxos::platform

namespace lexxos::devices {

class Stm32ParallelTftDisplay final : public DisplayDevice {
public:
	explicit Stm32ParallelTftDisplay(lexxos::platform::ParallelTftDisplayPins config);
	Stm32ParallelTftDisplay(lexxos::platform::ParallelTftDisplayPins config, lexxos::platform::PlatformServices const& platform);

	bool initialize();

	[[nodiscard]] std::string_view name() const override;
	[[nodiscard]] bool isReady() const override;
	[[nodiscard]] int width() const override;
	[[nodiscard]] int height() const override;
	[[nodiscard]] PixelFormat nativeFormat() const override;

	bool clear(std::uint16_t rgb565) override;
	bool beginFrame() override;
	bool blitRgb565(Rect const& rect, std::uint16_t const* pixels, int stridePixels) override;
	bool endFrame() override;

private:
	void _hardwareReset();
	void _setBacklight(bool enabled);
	[[nodiscard]] bool _isRectValid(Rect const& rect) const;
	[[nodiscard]] bool _configureOutput(lexxos::platform::GpioPin pin, bool initialValue) const;
	void _writeGpio(lexxos::platform::GpioPin pin, bool value) const;
	void _sleepMs(std::uint32_t milliseconds) const;

	lexxos::platform::PlatformServices const& _platformServices;
	lexxos::platform::ParallelTftDisplayPins _config;
	bool _ready{false};
	bool _frameActive{false};
};

} // namespace lexxos::devices
