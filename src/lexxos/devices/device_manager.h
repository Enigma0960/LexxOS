#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/devices/audio/audio_registry.h>
#include <lexxos/devices/detail/device_resource_registry.h>
#include <lexxos/devices/device_resource.h>
#include <lexxos/runtime/runtime_profile.h>

namespace lexxos::platform {
struct PlatformServices;
} // namespace lexxos::platform

namespace lexxos::devices {

class AudioDevice;
class BlockDevice;
class DisplayDevice;
class GamepadDevice;
class I2cBus;
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

class DeviceResourceLock {
public:
	DeviceResourceLock() = default;
	DeviceResourceLock(DeviceResourceLock const&) = delete;
	DeviceResourceLock& operator=(DeviceResourceLock const&) = delete;
	DeviceResourceLock(DeviceResourceLock&& other) noexcept;
	DeviceResourceLock& operator=(DeviceResourceLock&& other) noexcept;
	~DeviceResourceLock();
	[[nodiscard]] bool valid() const;
	[[nodiscard]] explicit operator bool() const;
	void release();

private:
	friend class DeviceManager;
	enum class Kind : std::uint8_t {
		None,
		I2cBus,
		BlockDevice,
		DisplayDevice,
		InputDevice,
		PointingDevice,
		GamepadDevice,
		SensorDevice,
		InfraredDevice,
		PicoCalcSouthbridgeService,
	};
	DeviceResourceLock(DeviceManager& manager, Kind kind, std::size_t index);
	DeviceManager* _manager{nullptr};
	Kind _kind{Kind::None};
	std::size_t _index{0};
};

class DeviceManager {
public:
	static constexpr std::size_t maxI2cBuses = lexxos::runtime::standardRuntimeProfile.devices.maxI2cBuses;
	static constexpr std::size_t maxBlockDevices = lexxos::runtime::standardRuntimeProfile.devices.maxBlockDevices;
	static constexpr std::size_t maxDisplayDevices = lexxos::runtime::standardRuntimeProfile.devices.maxDisplayDevices;
	static constexpr std::size_t maxInputDevices = lexxos::runtime::standardRuntimeProfile.devices.maxInputDevices;
	static constexpr std::size_t maxPointingDevices = lexxos::runtime::standardRuntimeProfile.devices.maxPointingDevices;
	static constexpr std::size_t maxGamepadDevices = lexxos::runtime::standardRuntimeProfile.devices.maxGamepadDevices;
	static constexpr std::size_t maxAudioDevices = lexxos::runtime::standardRuntimeProfile.devices.maxAudioDevices;
	static constexpr std::size_t maxMicrophoneDevices = lexxos::runtime::standardRuntimeProfile.devices.maxMicrophoneDevices;
	static constexpr std::size_t maxSensorDevices = lexxos::runtime::standardRuntimeProfile.devices.maxSensorDevices;
	static constexpr std::size_t maxInfraredDevices = lexxos::runtime::standardRuntimeProfile.devices.maxInfraredDevices;
	static constexpr std::size_t maxPicoCalcSouthbridgeServices =
	    lexxos::runtime::standardRuntimeProfile.devices.maxPicoCalcSouthbridgeServices;

	void initialize();
	void initialize(lexxos::platform::PlatformServices const& platform);

	bool registerI2cBus(std::string_view name, std::string_view description, I2cBus& bus);
	bool registerBlockDevice(std::string_view name, std::string_view description, BlockDevice& device);
	bool registerDisplayDevice(std::string_view name, std::string_view description, DisplayDevice& device);
	bool registerInputDevice(std::string_view name, std::string_view description, InputDevice& device);
	bool registerPointingDevice(std::string_view name, std::string_view description, PointingDevice& device);
	bool registerGamepadDevice(std::string_view name, std::string_view description, GamepadDevice& device);
	bool registerSensorDevice(std::string_view name, std::string_view description, Sensor& device);
	bool registerInfraredDevice(std::string_view name, std::string_view description, InfraredTransmitter& device);
	bool registerPicoCalcSouthbridgeService(std::string_view name, std::string_view description, PicoCalcSouthbridgeRuntimeService& service);

	[[nodiscard]] I2cBus* i2cBus(std::string_view name);
	[[nodiscard]] I2cBus const* i2cBus(std::string_view name) const;
	[[nodiscard]] std::size_t i2cBusCount() const;
	[[nodiscard]] I2cBusDescriptor const& i2cBusAt(std::size_t index) const;

	[[nodiscard]] bool isI2cBusLocked(std::string_view name) const;
	[[nodiscard]] bool isI2cBusLockedAt(std::size_t index) const;
	[[nodiscard]] DeviceResourceStatus i2cBusStatusAt(std::size_t index) const;
	[[nodiscard]] DeviceResourceLock lockI2cBus(
	    std::string_view name,
	    DeviceResourcePriority priority = DeviceResourcePriority::Normal,
	    std::string_view owner = {});

	[[nodiscard]] BlockDevice* blockDevice(std::string_view name);
	[[nodiscard]] BlockDevice const* blockDevice(std::string_view name) const;
	[[nodiscard]] std::size_t blockDeviceCount() const;
	[[nodiscard]] BlockDeviceDescriptor const& blockDeviceAt(std::size_t index) const;

	[[nodiscard]] bool isBlockDeviceLocked(std::string_view name) const;
	[[nodiscard]] bool isBlockDeviceLockedAt(std::size_t index) const;
	[[nodiscard]] DeviceResourceStatus blockDeviceStatusAt(std::size_t index) const;
	[[nodiscard]] DeviceResourceLock lockBlockDevice(
	    std::string_view name,
	    DeviceResourcePriority priority = DeviceResourcePriority::Normal,
	    std::string_view owner = {});

	[[nodiscard]] DisplayDevice* displayDevice(std::string_view name);
	[[nodiscard]] DisplayDevice const* displayDevice(std::string_view name) const;
	[[nodiscard]] DisplayDevice* primaryDisplayDevice();
	[[nodiscard]] DisplayDevice const* primaryDisplayDevice() const;
	[[nodiscard]] std::size_t displayDeviceCount() const;
	[[nodiscard]] DisplayDeviceDescriptor const& displayDeviceAt(std::size_t index) const;
	[[nodiscard]] bool isDisplayDeviceLocked(std::string_view name) const;
	[[nodiscard]] bool isDisplayDeviceLockedAt(std::size_t index) const;
	[[nodiscard]] DeviceResourceStatus displayDeviceStatusAt(std::size_t index) const;
	[[nodiscard]] DeviceResourceLock lockDisplayDevice(
	    std::string_view name,
	    DeviceResourcePriority priority = DeviceResourcePriority::Normal,
	    std::string_view owner = {});

	[[nodiscard]] InputDevice* inputDevice(std::string_view name);
	[[nodiscard]] InputDevice const* inputDevice(std::string_view name) const;
	[[nodiscard]] InputDevice* primaryInputDevice();
	[[nodiscard]] InputDevice const* primaryInputDevice() const;
	[[nodiscard]] std::size_t inputDeviceCount() const;
	[[nodiscard]] InputDeviceDescriptor const& inputDeviceAt(std::size_t index) const;
	[[nodiscard]] bool isInputDeviceLocked(std::string_view name) const;
	[[nodiscard]] bool isInputDeviceLockedAt(std::size_t index) const;
	[[nodiscard]] DeviceResourceStatus inputDeviceStatusAt(std::size_t index) const;
	[[nodiscard]] DeviceResourceLock lockInputDevice(
	    std::string_view name,
	    DeviceResourcePriority priority = DeviceResourcePriority::Normal,
	    std::string_view owner = {});

	[[nodiscard]] PointingDevice* pointingDevice(std::string_view name);
	[[nodiscard]] PointingDevice const* pointingDevice(std::string_view name) const;
	[[nodiscard]] PointingDevice* primaryPointingDevice();
	[[nodiscard]] PointingDevice const* primaryPointingDevice() const;
	[[nodiscard]] std::size_t pointingDeviceCount() const;
	[[nodiscard]] PointingDeviceDescriptor const& pointingDeviceAt(std::size_t index) const;
	[[nodiscard]] bool isPointingDeviceLocked(std::string_view name) const;
	[[nodiscard]] bool isPointingDeviceLockedAt(std::size_t index) const;
	[[nodiscard]] DeviceResourceStatus pointingDeviceStatusAt(std::size_t index) const;
	[[nodiscard]] DeviceResourceLock lockPointingDevice(
	    std::string_view name,
	    DeviceResourcePriority priority = DeviceResourcePriority::Normal,
	    std::string_view owner = {});

	[[nodiscard]] GamepadDevice* gamepadDevice(std::string_view name);
	[[nodiscard]] GamepadDevice const* gamepadDevice(std::string_view name) const;
	[[nodiscard]] GamepadDevice* primaryGamepadDevice();
	[[nodiscard]] GamepadDevice const* primaryGamepadDevice() const;
	[[nodiscard]] std::size_t gamepadDeviceCount() const;
	[[nodiscard]] GamepadDeviceDescriptor const& gamepadDeviceAt(std::size_t index) const;
	[[nodiscard]] bool isGamepadDeviceLocked(std::string_view name) const;
	[[nodiscard]] bool isGamepadDeviceLockedAt(std::size_t index) const;
	[[nodiscard]] DeviceResourceStatus gamepadDeviceStatusAt(std::size_t index) const;
	[[nodiscard]] DeviceResourceLock lockGamepadDevice(
	    std::string_view name,
	    DeviceResourcePriority priority = DeviceResourcePriority::Normal,
	    std::string_view owner = {});

	[[nodiscard]] Sensor* sensorDevice(std::string_view name);
	[[nodiscard]] Sensor const* sensorDevice(std::string_view name) const;
	[[nodiscard]] Sensor* primarySensorDevice();
	[[nodiscard]] Sensor const* primarySensorDevice() const;
	[[nodiscard]] std::size_t sensorDeviceCount() const;
	[[nodiscard]] SensorDeviceDescriptor const& sensorDeviceAt(std::size_t index) const;
	[[nodiscard]] bool isSensorDeviceLocked(std::string_view name) const;
	[[nodiscard]] bool isSensorDeviceLockedAt(std::size_t index) const;
	[[nodiscard]] DeviceResourceStatus sensorDeviceStatusAt(std::size_t index) const;
	[[nodiscard]] DeviceResourceLock lockSensorDevice(
	    std::string_view name,
	    DeviceResourcePriority priority = DeviceResourcePriority::Normal,
	    std::string_view owner = {});

	[[nodiscard]] InfraredTransmitter* infraredDevice(std::string_view name);
	[[nodiscard]] InfraredTransmitter const* infraredDevice(std::string_view name) const;
	[[nodiscard]] InfraredTransmitter* primaryInfraredDevice();
	[[nodiscard]] InfraredTransmitter const* primaryInfraredDevice() const;
	[[nodiscard]] std::size_t infraredDeviceCount() const;
	[[nodiscard]] InfraredDeviceDescriptor const& infraredDeviceAt(std::size_t index) const;
	[[nodiscard]] bool isInfraredDeviceLocked(std::string_view name) const;
	[[nodiscard]] bool isInfraredDeviceLockedAt(std::size_t index) const;
	[[nodiscard]] DeviceResourceStatus infraredDeviceStatusAt(std::size_t index) const;
	[[nodiscard]] DeviceResourceLock lockInfraredDevice(
	    std::string_view name,
	    DeviceResourcePriority priority = DeviceResourcePriority::Normal,
	    std::string_view owner = {});

	[[nodiscard]] AudioDevice* audioDevice(lexxos::platform::PlatformServices const& platform, std::string_view name);
	[[nodiscard]] AudioDevice const* audioDevice(lexxos::platform::PlatformServices const& platform, std::string_view name) const;
	[[nodiscard]] AudioDevice* primaryAudioDevice(lexxos::platform::PlatformServices const& platform);
	[[nodiscard]] AudioDevice const* primaryAudioDevice(lexxos::platform::PlatformServices const& platform) const;
	[[nodiscard]] std::size_t audioDeviceCount(lexxos::platform::PlatformServices const& platform) const;
	[[nodiscard]] AudioDeviceDescriptor audioDeviceAt(lexxos::platform::PlatformServices const& platform, std::size_t index) const;
	[[nodiscard]] DeviceResourceStatus audioDeviceStatusAt(lexxos::platform::PlatformServices const& platform, std::size_t index) const;

	[[nodiscard]] MicrophoneDevice* microphoneDevice(lexxos::platform::PlatformServices const& platform, std::string_view name);
	[[nodiscard]] MicrophoneDevice const* microphoneDevice(lexxos::platform::PlatformServices const& platform, std::string_view name) const;
	[[nodiscard]] MicrophoneDevice* primaryMicrophoneDevice(lexxos::platform::PlatformServices const& platform);
	[[nodiscard]] MicrophoneDevice const* primaryMicrophoneDevice(lexxos::platform::PlatformServices const& platform) const;
	[[nodiscard]] std::size_t microphoneDeviceCount(lexxos::platform::PlatformServices const& platform) const;
	[[nodiscard]] MicrophoneDeviceDescriptor microphoneDeviceAt(lexxos::platform::PlatformServices const& platform, std::size_t index) const;
	[[nodiscard]] DeviceResourceStatus microphoneDeviceStatusAt(lexxos::platform::PlatformServices const& platform, std::size_t index) const;

	[[nodiscard]] PicoCalcSouthbridgeRuntimeService* picoCalcSouthbridgeService(std::string_view name);
	[[nodiscard]] PicoCalcSouthbridgeRuntimeService const* picoCalcSouthbridgeService(std::string_view name) const;
	[[nodiscard]] std::size_t picoCalcSouthbridgeServiceCount() const;
	[[nodiscard]] PicoCalcSouthbridgeServiceDescriptor const& picoCalcSouthbridgeServiceAt(std::size_t index) const;
	[[nodiscard]] bool isPicoCalcSouthbridgeServiceLocked(std::string_view name) const;
	[[nodiscard]] bool isPicoCalcSouthbridgeServiceLockedAt(std::size_t index) const;
	[[nodiscard]] DeviceResourceStatus picoCalcSouthbridgeServiceStatusAt(std::size_t index) const;
	[[nodiscard]] DeviceResourceLock lockPicoCalcSouthbridgeService(
	    std::string_view name,
	    DeviceResourcePriority priority = DeviceResourcePriority::Normal,
	    std::string_view owner = {});

private:
	friend class DeviceResourceLock;
	void _release(DeviceResourceLock::Kind kind, std::size_t index);

	detail::DeviceResourceRegistry<I2cBusDescriptor, maxI2cBuses> _i2cBuses{};
	detail::DeviceResourceRegistry<BlockDeviceDescriptor, maxBlockDevices> _blockDevices{};
	detail::DeviceResourceRegistry<DisplayDeviceDescriptor, maxDisplayDevices> _displayDevices{};
	detail::DeviceResourceRegistry<InputDeviceDescriptor, maxInputDevices> _inputDevices{};
	detail::DeviceResourceRegistry<PointingDeviceDescriptor, maxPointingDevices> _pointingDevices{};
	detail::DeviceResourceRegistry<GamepadDeviceDescriptor, maxGamepadDevices> _gamepadDevices{};
	detail::DeviceResourceRegistry<SensorDeviceDescriptor, maxSensorDevices> _sensorDevices{};
	detail::DeviceResourceRegistry<InfraredDeviceDescriptor, maxInfraredDevices> _infraredDevices{};
	detail::DeviceResourceRegistry<PicoCalcSouthbridgeServiceDescriptor, maxPicoCalcSouthbridgeServices>
	    _picoCalcSouthbridgeServices{};
};

inline AudioDevice* DeviceManager::audioDevice(lexxos::platform::PlatformServices const& platform, std::string_view name) {
	auto* device = primaryAudioDevice(platform);
	return device != nullptr && device->name() == name ? device : nullptr;
}

inline AudioDevice const* DeviceManager::audioDevice(lexxos::platform::PlatformServices const& platform, std::string_view name) const {
	auto const* device = primaryAudioDevice(platform);
	return device != nullptr && device->name() == name ? device : nullptr;
}

inline AudioDevice* DeviceManager::primaryAudioDevice(lexxos::platform::PlatformServices const& platform) {
	return lexxos::devices::audio::primaryAudioDevice(platform);
}

inline AudioDevice const* DeviceManager::primaryAudioDevice(lexxos::platform::PlatformServices const& platform) const {
	return lexxos::devices::audio::primaryAudioDevice(platform);
}

inline std::size_t DeviceManager::audioDeviceCount(lexxos::platform::PlatformServices const& platform) const {
	return primaryAudioDevice(platform) != nullptr ? 1 : 0;
}

inline AudioDeviceDescriptor DeviceManager::audioDeviceAt(lexxos::platform::PlatformServices const& platform, std::size_t index) const {
	auto* device = lexxos::devices::audio::primaryAudioDevice(platform);
	if (index != 0 || device == nullptr) {
		return {};
	}
	return {
	    .name = device->name(),
	    .description = "primary board audio output",
	    .device = device,
	};
}

inline DeviceResourceStatus DeviceManager::audioDeviceStatusAt(lexxos::platform::PlatformServices const& platform, std::size_t index) const {
	(void) platform;
	(void) index;
	return {};
}

inline MicrophoneDevice* DeviceManager::microphoneDevice(lexxos::platform::PlatformServices const& platform, std::string_view name) {
	auto* device = primaryMicrophoneDevice(platform);
	return device != nullptr && device->name() == name ? device : nullptr;
}

inline MicrophoneDevice const* DeviceManager::microphoneDevice(lexxos::platform::PlatformServices const& platform, std::string_view name) const {
	auto const* device = primaryMicrophoneDevice(platform);
	return device != nullptr && device->name() == name ? device : nullptr;
}

inline MicrophoneDevice* DeviceManager::primaryMicrophoneDevice(lexxos::platform::PlatformServices const& platform) {
	return lexxos::devices::audio::primaryMicrophoneDevice(platform);
}

inline MicrophoneDevice const* DeviceManager::primaryMicrophoneDevice(lexxos::platform::PlatformServices const& platform) const {
	return lexxos::devices::audio::primaryMicrophoneDevice(platform);
}

inline std::size_t DeviceManager::microphoneDeviceCount(lexxos::platform::PlatformServices const& platform) const {
	return primaryMicrophoneDevice(platform) != nullptr ? 1 : 0;
}

inline MicrophoneDeviceDescriptor DeviceManager::microphoneDeviceAt(lexxos::platform::PlatformServices const& platform, std::size_t index) const {
	auto* device = lexxos::devices::audio::primaryMicrophoneDevice(platform);
	if (index != 0 || device == nullptr) {
		return {};
	}
	return {
	    .name = device->name(),
	    .description = "primary board microphone input",
	    .device = device,
	};
}

inline DeviceResourceStatus DeviceManager::microphoneDeviceStatusAt(
    lexxos::platform::PlatformServices const& platform,
    std::size_t index) const {
	(void) platform;
	(void) index;
	return {};
}

DeviceManager& deviceManager();

} // namespace lexxos::devices
