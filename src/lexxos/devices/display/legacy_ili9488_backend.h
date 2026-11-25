#pragma once

#include <cstddef>

#include <lexxos/devices/display/ili9488_backend.h>
#include <lexxos/platform/board.h>

namespace lexxos::platform {

struct PlatformServices;

} // namespace lexxos::platform

namespace lexxos::devices {

// Совместимый аппаратный адаптер; caller удерживает шину до завершения write.
class LegacyIli9488Backend final : public Ili9488Backend {
public:
	LegacyIli9488Backend(platform::SpiTftDisplayPins config, platform::PlatformServices const& services);
	~LegacyIli9488Backend() override;
	LegacyIli9488Backend(LegacyIli9488Backend const&) = delete;
	LegacyIli9488Backend& operator=(LegacyIli9488Backend const&) = delete;
	bool initialize() override;
	bool write(std::uint8_t const* data, std::size_t size) override;
	bool supportsRgb565Words() const override;
	bool writeRgb565Words(std::uint16_t const* data, std::size_t pixels) override;
	bool configureBusSpeed(std::uint32_t hz) override;

	bool hardwareSpi() const override { return _hardwareSpi; }

	bool configureOutput(platform::GpioPin pin, bool initialValue) const;
	void writeGpio(platform::GpioPin pin, bool value) const;
	void sleepMs(std::uint32_t milliseconds) const override;
	std::uint32_t busSpeedHz() const override;
	void select(bool selected) override;
	void dataMode(bool data) override;
	void resetLine(bool high) override;

private:
	platform::SpiTftDisplayPins _config;
	platform::PlatformServices const& _platformServices;
	bool _hardwareSpi{false};
#if defined(LEXXOS_PLATFORM_PICO)
	int _dmaChannel{-1};
#endif
};

} // namespace lexxos::devices
