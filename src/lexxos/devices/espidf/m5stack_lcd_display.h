#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/devices/bus/spi_bus.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/platform/board.h>

namespace lexxos::platform {
struct BoardDescriptor;
struct PlatformServices;
} // namespace lexxos::platform

namespace lexxos::devices {

class M5StackLcdDisplay final : public DisplayDevice {
public:
	M5StackLcdDisplay(SpiBus& bus, lexxos::platform::SpiTftDisplayPins config);
	M5StackLcdDisplay(SpiBus& bus, lexxos::platform::SpiTftDisplayPins config, lexxos::platform::PlatformServices const& platform);

	bool initialize();

	[[nodiscard]] std::string_view name() const override;
	[[nodiscard]] bool isReady() const override;
	[[nodiscard]] int width() const override;
	[[nodiscard]] int height() const override;
	[[nodiscard]] PixelFormat nativeFormat() const override;

	bool clear(std::uint16_t rgb565) override;
	bool beginFrame() override;
	bool blitRgb565(Rect const& rect, std::uint16_t const* pixels, int stridePixels) override;
	bool beginRgb565Stream(Rect const& rect) override;
	bool writeRgb565Stream(std::uint16_t const* pixels, int count) override;
	bool writeRgb565BytesStream(std::uint8_t const* bytes, int pixelCount) override;
	bool endRgb565Stream() override;
	bool endFrame() override;

private:
	bool _configurePinsAndBus();
	bool _prepareBusForLcd();
	void _hardwareReset();
	bool _initializeController();
	bool _initializeIli934xController();
	bool _initializeSt7789Controller();
	bool _writeCommand(std::uint8_t command);
	bool _writeData(std::uint8_t value);
	bool _writeData(std::uint8_t const* data, std::size_t size);
	bool _setAddressWindow(Rect const& rect);
	[[nodiscard]] bool _isRectValid(Rect const& rect) const;
	void _setBacklight(bool enabled);

	bool _beginWrite(bool dataMode);
	void _endWrite();
	bool _writeBytes(std::uint8_t const* data, std::size_t size);
	bool _writePixels(std::uint16_t const* pixels, int count);
	bool _writeRepeatedPixel(std::uint16_t rgb565, int count);
	[[nodiscard]] lexxos::platform::BoardDescriptor const& _board() const;
	[[nodiscard]] bool _configureOutput(lexxos::platform::GpioPin pin, bool initialValue) const;
	void _writeGpio(lexxos::platform::GpioPin pin, bool value) const;
	void _sleepMs(std::uint32_t milliseconds) const;

	SpiBus& _bus;
	lexxos::platform::PlatformServices const& _platformServices;
	lexxos::platform::SpiTftDisplayPins _config;
	bool _ready{false};
	bool _frameActive{false};
	bool _rgb565StreamActive{false};
	bool _writeActive{false};
};

} // namespace lexxos::devices
