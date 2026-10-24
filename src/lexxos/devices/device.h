#pragma once

#include <string_view>

namespace lexxos::devices {

enum class DeviceKind {
	Unknown,
	I2cBus,
	BlockDevice,
	Display,
	Input,
	Gamepad,
	Audio,
	Microphone,
	Sensor,
	Infrared,
	Service,
	Pointing,
};

[[nodiscard]] constexpr std::string_view toString(DeviceKind kind) {
	switch (kind) {
		case DeviceKind::Unknown:
			return "unknown";
		case DeviceKind::I2cBus:
			return "i2c";
		case DeviceKind::BlockDevice:
			return "block";
		case DeviceKind::Display:
			return "display";
		case DeviceKind::Input:
			return "input";
		case DeviceKind::Gamepad:
			return "gamepad";
		case DeviceKind::Audio:
			return "audio";
		case DeviceKind::Microphone:
			return "microphone";
		case DeviceKind::Sensor:
			return "sensor";
		case DeviceKind::Infrared:
			return "ir";
		case DeviceKind::Service:
			return "service";
		case DeviceKind::Pointing:
			return "pointer";
	}
	return "unknown";
}

class Device {
public:
	virtual ~Device() = default;

	[[nodiscard]] virtual std::string_view name() const = 0;
	[[nodiscard]] virtual DeviceKind kind() const = 0;
	[[nodiscard]] virtual bool isReady() const = 0;
};

} // namespace lexxos::devices
