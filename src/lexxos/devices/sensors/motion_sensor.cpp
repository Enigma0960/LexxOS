#include <lexxos/devices/sensors/motion_sensor.h>

namespace lexxos::devices {

MotionSensor* MotionSensor::asMotionSensor() {
	return this;
}

MotionSensor const* MotionSensor::asMotionSensor() const {
	return this;
}

} // namespace lexxos::devices
