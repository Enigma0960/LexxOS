#pragma once

#include <cstdint>
#include <string_view>

#include <lexxos/devices/device.h>

namespace lexxos::devices {

class MotionSensor;

enum class SensorState : std::uint8_t {
	Unavailable,
	Ready,
	Fault,
};

[[nodiscard]] constexpr std::string_view toString(SensorState state) {
	switch (state) {
		case SensorState::Unavailable:
			return "unavailable";
		case SensorState::Ready:
			return "ready";
		case SensorState::Fault:
			return "fault";
	}

	return "unknown";
}

struct MotionSensorSample {
	bool valid{false};
	std::uint32_t timestampMs{0};
	std::uint8_t chipId{0};
	std::uint8_t status{0};
	std::uint8_t error{0};
	std::int16_t accelX{0};
	std::int16_t accelY{0};
	std::int16_t accelZ{0};
	std::int16_t gyroX{0};
	std::int16_t gyroY{0};
	std::int16_t gyroZ{0};
	std::int16_t temperatureCentiC{0};
};

class Sensor : public Device {
public:
	virtual ~Sensor() = default;

	[[nodiscard]] DeviceKind kind() const override;

	[[nodiscard]] bool isReady() const override;

	[[nodiscard]] virtual SensorState state() const = 0;
	virtual bool poll() = 0;

	[[nodiscard]] virtual MotionSensor* asMotionSensor();

	[[nodiscard]] virtual MotionSensor const* asMotionSensor() const;
};

} // namespace lexxos::devices

#include <lexxos/devices/sensors/motion_sensor.h>
