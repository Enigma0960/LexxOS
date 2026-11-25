#pragma once

#include <cstddef>
#include <string_view>

#include <lexxos/platform/board.h>

namespace lexxos::devices {

class AudioDevice;
class BlockDevice;
class DisplayDevice;
class GamepadDevice;
class I2cBus;
class SpiBus;
class UartBus;
class InfraredTransmitter;
class InputDevice;
class PointingDevice;
class MicrophoneDevice;
class Sensor;
class PicoCalcSouthbridgeRuntimeService;
class DeviceManager;

struct I2cBusDescriptor {
	std::string_view name;
	std::string_view description;
	I2cBus* bus{nullptr};
};

struct SpiBusDescriptor {
	std::string_view name;
	std::string_view description;
	SpiBus* bus{nullptr};
};

struct UartBusDescriptor {
	std::string_view name;
	std::string_view description;
	UartBus* bus{nullptr};
};

struct GpioPinDescriptor {
	std::string_view name;
	std::string_view description;
	lexxos::platform::GpioPin pin{};
	bool inputAllowed{false};
	bool outputAllowed{false};
};

struct BlockDeviceDescriptor {
	std::string_view name;
	std::string_view description;
	BlockDevice* device{nullptr};
};

struct DisplayDeviceDescriptor {
	std::string_view name;
	std::string_view description;
	DisplayDevice* device{nullptr};
};

struct InputDeviceDescriptor {
	std::string_view name;
	std::string_view description;
	InputDevice* device{nullptr};
};

struct PointingDeviceDescriptor {
	std::string_view name;
	std::string_view description;
	PointingDevice* device{nullptr};
};

struct GamepadDeviceDescriptor {
	std::string_view name;
	std::string_view description;
	GamepadDevice* device{nullptr};
};

struct AudioDeviceDescriptor {
	std::string_view name;
	std::string_view description;
	AudioDevice* device{nullptr};
};

struct MicrophoneDeviceDescriptor {
	std::string_view name;
	std::string_view description;
	MicrophoneDevice* device{nullptr};
};

struct SensorDeviceDescriptor {
	std::string_view name;
	std::string_view description;
	Sensor* device{nullptr};
};

struct InfraredDeviceDescriptor {
	std::string_view name;
	std::string_view description;
	InfraredTransmitter* device{nullptr};
};

struct PicoCalcSouthbridgeServiceDescriptor {
	std::string_view name;
	std::string_view description;
	PicoCalcSouthbridgeRuntimeService* service{nullptr};
};

} // namespace lexxos::devices
