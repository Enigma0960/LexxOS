#pragma once

#include <cstdint>
#include <string_view>

#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/platform/board.h>

namespace lexxos::devices {

class EspIdfMipiDsiDisplay final : public DisplayDevice {
public:
	EspIdfMipiDsiDisplay(I2cBus& controlBus, lexxos::platform::MipiDsiDisplayConfig config);

	bool initialize();

	[[nodiscard]] std::string_view name() const override;
	[[nodiscard]] bool isReady() const override;
	[[nodiscard]] int width() const override;
	[[nodiscard]] int height() const override;
	[[nodiscard]] PixelFormat nativeFormat() const override;
	[[nodiscard]] DisplayWireFormat wireFormat() const override;
	[[nodiscard]] int wireBytesPerPixel() const override;
	[[nodiscard]] std::uint32_t busSpeedHz() const override;
	[[nodiscard]] bool supportsPowerMode() const override;
	[[nodiscard]] DisplayPowerMode powerMode() const override;
	bool setPowerMode(DisplayPowerMode mode) override;
	[[nodiscard]] bool supportsBacklight() const override;
	[[nodiscard]] std::uint8_t backlight() const override;
	bool setBacklight(std::uint8_t value) override;

	bool clear(std::uint16_t rgb565) override;
	bool beginFrame() override;
	bool blitRgb565(Rect const& rect, std::uint16_t const* pixels, int stridePixels) override;
	bool fillRgb565(Rect const& rect, std::uint16_t rgb565) override;
	bool beginRgb565Stream(Rect const& rect) override;
	bool writeRgb565Stream(std::uint16_t const* pixels, int count) override;
	bool writeRgb565BytesStream(std::uint8_t const* bytes, int pixelCount) override;
	bool endRgb565Stream() override;
	bool endFrame() override;

private:
	bool _initializeDsi();
	bool _enablePhyPower();
	bool _initializePowerControl();
	bool _writePowerRegister(std::uint8_t reg, std::uint8_t value);
	bool _releaseBridgeResetAndWake();
	bool _setBrightness(std::uint8_t brightness);
	bool _initializeTc358762Bridge();
	void _writeTc358762(std::uint16_t reg, std::uint32_t value);
	bool _flush(Rect const& rect);
	bool _copyToFrameBuffer(Rect const& rect, std::uint16_t const* pixels, int stridePixels);
	bool _copyBytesToFrameBuffer(Rect const& rect, std::uint8_t const* bytes, int stridePixels);
	[[nodiscard]] int _frameBufferBytesPerPixel() const;
	[[nodiscard]] std::uint8_t* _frameBufferPixel(int x, int y);
	void _writeFrameBufferPixel(int x, int y, std::uint16_t rgb565);
	bool _isRectValid(Rect const& rect) const;
	void _sleepMs(std::uint32_t milliseconds) const;

	I2cBus& _controlBus;
	lexxos::platform::MipiDsiDisplayConfig _config;
	void* _dsiBus{nullptr};
	void* _dbiIo{nullptr};
	void* _panel{nullptr};
	void* _phyLdo{nullptr};
	std::uint8_t* _frameBuffer{nullptr};
	Rect _streamRect{};
	int _streamOffsetPixels{0};
	DisplayPowerMode _powerMode{DisplayPowerMode::On};
	std::uint8_t _backlight{0};
	bool _ready{false};
	bool _frameActive{false};
	bool _streamActive{false};
	bool _powerControlReady{false};
};

} // namespace lexxos::devices
