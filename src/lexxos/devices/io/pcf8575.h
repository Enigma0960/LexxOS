#pragma once

#include <cstdint>
#include <string_view>

#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/platform/board.h>

namespace lexxos::devices {

class Pcf8575PortExpander {
public:
	Pcf8575PortExpander(I2cBus& bus, lexxos::platform::Pcf8575Pins config);

	bool initialize();
	[[nodiscard]] std::string_view name() const;
	[[nodiscard]] bool isReady() const;
	[[nodiscard]] std::uint8_t address() const;
	[[nodiscard]] lexxos::platform::GpioPin interruptPin() const;
	[[nodiscard]] I2cBusTransaction transaction();

	bool writePort(std::uint16_t value);
	bool readPort(std::uint16_t& value);
	bool releasePins(std::uint16_t mask);
	[[nodiscard]] std::uint16_t lastWrittenPort() const;

private:
	I2cBus& _bus;
	lexxos::platform::Pcf8575Pins _config;
	std::uint16_t _lastWrittenPort{0xFFFF};
	bool _ready{false};
};

} // namespace lexxos::devices
