#pragma once

#include <cstdint>

#include <lexxos/devices/device.h>

namespace lexxos::devices {

enum class PixelFormat {
	Unknown,
	Rgb565,
	Grayscale8,
	Indexed2,
};

enum class DisplayWireFormat : std::uint8_t {
	Unknown,
	Rgb565,
	Rgb666,
	Rgb888,
};

enum class DisplayPowerMode : std::uint8_t {
	On,
	Off,
	Sleep,
};

[[nodiscard]] constexpr std::string_view toString(PixelFormat format) {
	switch (format) {
		case PixelFormat::Unknown:
			return "unknown";
		case PixelFormat::Rgb565:
			return "rgb565";
		case PixelFormat::Grayscale8:
			return "gray8";
		case PixelFormat::Indexed2:
			return "indexed2";
	}
	return "unknown";
}

[[nodiscard]] constexpr std::string_view toString(DisplayWireFormat format) {
	switch (format) {
		case DisplayWireFormat::Unknown:
			return "unknown";
		case DisplayWireFormat::Rgb565:
			return "rgb565";
		case DisplayWireFormat::Rgb666:
			return "rgb666";
		case DisplayWireFormat::Rgb888:
			return "rgb888";
	}
	return "unknown";
}

[[nodiscard]] constexpr std::string_view toString(DisplayPowerMode mode) {
	switch (mode) {
		case DisplayPowerMode::On:
			return "on";
		case DisplayPowerMode::Off:
			return "off";
		case DisplayPowerMode::Sleep:
			return "sleep";
	}
	return "unknown";
}

struct Rect {
	int x{0};
	int y{0};
	int width{0};
	int height{0};
};

class DisplayDevice : public Device {
public:
	[[nodiscard]] DeviceKind kind() const override;

	[[nodiscard]] virtual int width() const = 0;
	[[nodiscard]] virtual int height() const = 0;
	[[nodiscard]] virtual PixelFormat nativeFormat() const = 0;
	[[nodiscard]] virtual DisplayWireFormat wireFormat() const;
	[[nodiscard]] virtual int wireBytesPerPixel() const;

	[[nodiscard]] virtual std::uint32_t busSpeedHz() const;
	virtual bool configureBusSpeed(std::uint32_t hz);

	[[nodiscard]] virtual bool supportsPowerMode() const;
	[[nodiscard]] virtual DisplayPowerMode powerMode() const;
	virtual bool setPowerMode(DisplayPowerMode mode);
	[[nodiscard]] virtual bool supportsBacklight() const;
	[[nodiscard]] virtual std::uint8_t backlight() const;
	virtual bool setBacklight(std::uint8_t value);

	virtual bool clear(std::uint16_t rgb565) = 0;
	virtual bool beginFrame() = 0;
	virtual bool blitRgb565(Rect const& rect, std::uint16_t const* pixels, int stridePixels) = 0;
	virtual bool fillRgb565(Rect const& rect, std::uint16_t rgb565);
	virtual bool endFrame() = 0;

	virtual bool beginRgb565Stream(Rect const& rect);
	virtual bool writeRgb565Stream(std::uint16_t const* pixels, int count);
	virtual bool writeRgb565BytesStream(std::uint8_t const* bytes, int pixelCount);
	virtual bool writeRgb888Stream(std::uint8_t const* bytes, int pixelCount);
	virtual bool endRgb565Stream();

	virtual bool configureVerticalScroll(int topFixedPixels, int scrollAreaPixels, int bottomFixedPixels);
	virtual bool setVerticalScrollStart(int startPixel);
};

} // namespace lexxos::devices
