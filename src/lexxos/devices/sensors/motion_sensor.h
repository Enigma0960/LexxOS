#pragma once

#include <lexxos/devices/sensors/sensor.h>

namespace lexxos::devices {

class MotionSensor : public Sensor {
public:
	[[nodiscard]] MotionSensor* asMotionSensor() override;

	[[nodiscard]] MotionSensor const* asMotionSensor() const override;

	[[nodiscard]] virtual MotionSensorSample lastSample() const = 0;
	virtual bool readSample(MotionSensorSample& sample) = 0;
};

} // namespace lexxos::devices
