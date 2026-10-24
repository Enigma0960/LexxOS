#include <lexxos/devices/gamepad_device.h>

namespace lexxos::devices {

DeviceKind GamepadDevice::kind() const {
	return DeviceKind::Gamepad;
}

} // namespace lexxos::devices
