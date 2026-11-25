#pragma once

#include <cstddef>
#include <cstdint>

namespace lexxos::devices {

class DeviceManager;

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
		SpiBus,
		UartBus,
		GpioPin,
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

} // namespace lexxos::devices
