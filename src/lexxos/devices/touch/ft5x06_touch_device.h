#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/pointing_device.h>
#include <lexxos/platform/board.h>
#include <lexxos/platform/platform_services.h>

namespace lexxos::devices {

class Ft5x06TouchDevice final : public PointingDevice {
public:
	Ft5x06TouchDevice(
	    I2cBus& bus,
	    lexxos::platform::TouchPanelConfig config,
	    lexxos::platform::PlatformServices const& platform);

	bool initialize();

	[[nodiscard]] std::string_view name() const override;
	[[nodiscard]] bool isReady() const override;
	[[nodiscard]] PointingDeviceState state() const override;
	[[nodiscard]] bool supportsTouch() const override;
	[[nodiscard]] TouchDeviceState touchState() const override;
	[[nodiscard]] TouchCalibration touchCalibration() const override;
	bool applyTouchCalibration(TouchCalibration const& calibration) override;
	void resetPosition() override;

private:
	bool _tryAddress(std::uint8_t address);
	bool _readRegister(std::uint8_t address, std::uint8_t reg, std::uint8_t* data, std::size_t size) const;
	bool _readRegister(std::uint8_t reg, std::uint8_t* data, std::size_t size) const;
	void _poll() const;
	void _setDefaultCalibration();
	[[nodiscard]] TouchContact _mapContact(std::uint8_t id, std::uint16_t rawX, std::uint16_t rawY, std::uint16_t size) const;
	[[nodiscard]] std::uint32_t _uptimeMs() const;

	I2cBus& _bus;
	lexxos::platform::TouchPanelConfig _config;
	lexxos::platform::PlatformServices const& _platform;
	std::uint8_t _address{0};
	bool _ready{false};
	mutable TouchDeviceState _touch{};
};

} // namespace lexxos::devices
