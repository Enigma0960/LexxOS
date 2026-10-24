#include <lexxos/devices/pointing_device.h>

namespace lexxos::devices {

DeviceKind PointingDevice::kind() const {
	return DeviceKind::Pointing;
}

bool PointingDevice::supportsTouch() const {
	return false;
}

TouchDeviceState PointingDevice::touchState() const {
	return {};
}

TouchCalibration PointingDevice::touchCalibration() const {
	return {};
}

bool PointingDevice::applyTouchCalibration(TouchCalibration const&) {
	return false;
}

} // namespace lexxos::devices
