#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/devices/display_device.h>
#include <lexxos/platform/board.h>

namespace lexxos::platform {
struct PlatformServices;
} // namespace lexxos::platform

namespace lexxos::devices {

class Ili9488Display final : public DisplayDevice {
public:
	explicit Ili9488Display(lexxos::platform::SpiTftDisplayPins config);
	Ili9488Display(lexxos::platform::SpiTftDisplayPins config, lexxos::platform::PlatformServices const& platform);

	bool initialize();

	[[nodiscard]] std::string_view name() const override;
	[[nodiscard]] bool isReady() const override;
	[[nodiscard]] int width() const override;
	[[nodiscard]] int height() const override;
	[[nodiscard]] PixelFormat nativeFormat() const override;
	[[nodiscard]] DisplayWireFormat wireFormat() const override;
	[[nodiscard]] int wireBytesPerPixel() const override;
	[[nodiscard]] std::uint32_t busSpeedHz() const override;
	bool configureBusSpeed(std::uint32_t hz) override;
	[[nodiscard]] bool supportsPowerMode() const override;
	[[nodiscard]] DisplayPowerMode powerMode() const override;

	bool setPowerMode(DisplayPowerMode mode) override;
	bool clear(std::uint16_t rgb565) override;
	bool beginFrame() override;
	bool blitRgb565(Rect const& rect, std::uint16_t const* pixels, int stridePixels) override;
	bool fillRgb565(Rect const& rect, std::uint16_t rgb565) override;
	bool beginRgb565Stream(Rect const& rect) override;
	bool writeRgb565Stream(std::uint16_t const* pixels, int count) override;
	bool writeRgb565BytesStream(std::uint8_t const* bytes, int pixelCount) override;
	bool writeRgb888Stream(std::uint8_t const* bytes, int pixelCount) override;
	bool endRgb565Stream() override;
	bool configureVerticalScroll(int topFixedPixels, int scrollAreaPixels, int bottomFixedPixels) override;
	bool setVerticalScrollStart(int startPixel) override;
	bool endFrame() override;

private:
	bool _configurePins();
	void _hardwareReset();
	void _initializeController();
	void _select();
	void _deselect();
	void _writeBytes(std::uint8_t const* data, std::size_t size);
	void _writeRepeatedByte(std::uint8_t value, std::size_t size);
	void _writePixels(std::uint16_t const* pixels, int count);
	void _writeRepeatedPixel(std::uint16_t rgb565, int count);
	void _writeCommand(std::uint8_t command);
	void _writeData(std::uint8_t value);
	void _writeData(std::uint8_t const* data, std::size_t size);
	bool _setAddressWindow(Rect const& rect);
	[[nodiscard]] bool _isRectValid(Rect const& rect) const;
	[[nodiscard]] bool _configureOutput(lexxos::platform::GpioPin pin, bool initialValue) const;
	void _writeGpio(lexxos::platform::GpioPin pin, bool value) const;
	void _sleepMs(std::uint32_t milliseconds) const;

	lexxos::platform::PlatformServices const& _platformServices;
	lexxos::platform::SpiTftDisplayPins _config;
	bool _ready{false};
	bool _frameActive{false};
	bool _rgb565StreamActive{false};
	bool _hardwareSpi{false};
	int _verticalScrollAreaPixels{0};
	int _verticalScrollTopFixedPixels{0};
	DisplayPowerMode _powerMode{DisplayPowerMode::On};

#if defined(LEXXOS_PLATFORM_PICO)
	int _dmaChannel{-1};
#endif
};

} // namespace lexxos::devices
