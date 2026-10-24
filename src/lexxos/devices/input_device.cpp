#include <lexxos/devices/input_device.h>

namespace lexxos::devices {

DeviceKind InputDevice::kind() const {
	return DeviceKind::Input;
}

void InputDevice::update() {
}

void InputDevice::setRawEventReporting(bool) {
}

std::uint32_t InputDevice::rawState() const {
	return 0;
}

std::uint32_t InputDevice::rawKeyCount() const {
	return 0;
}

std::uint32_t InputDevice::rawRowCount() const {
	return 0;
}

std::uint32_t InputDevice::rawColumnCount() const {
	return 0;
}

} // namespace lexxos::devices
