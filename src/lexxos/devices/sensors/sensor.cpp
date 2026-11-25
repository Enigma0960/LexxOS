#include <lexxos/devices/sensors/sensor.h>

namespace lexxos::devices {

DeviceKind Sensor::kind() const {
	return DeviceKind::Sensor;
}

bool Sensor::isReady() const {
	return state() == SensorState::Ready;
}

MotionSensor* Sensor::asMotionSensor() {
	return nullptr;
}

MotionSensor const* Sensor::asMotionSensor() const {
	return nullptr;
}

} // namespace lexxos::devices
