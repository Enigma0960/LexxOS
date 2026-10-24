#include <lexxos/devices/display_device.h>

namespace lexxos::devices {

DeviceKind DisplayDevice::kind() const {
	return DeviceKind::Display;
}

std::uint32_t DisplayDevice::busSpeedHz() const {
	return 0;
}

bool DisplayDevice::configureBusSpeed(std::uint32_t) {
	return false;
}

DisplayWireFormat DisplayDevice::wireFormat() const {
	return DisplayWireFormat::Unknown;
}

int DisplayDevice::wireBytesPerPixel() const {
	return 0;
}

bool DisplayDevice::supportsPowerMode() const {
	return false;
}

DisplayPowerMode DisplayDevice::powerMode() const {
	return DisplayPowerMode::On;
}

bool DisplayDevice::setPowerMode(DisplayPowerMode) {
	return false;
}

bool DisplayDevice::supportsBacklight() const {
	return false;
}

std::uint8_t DisplayDevice::backlight() const {
	return 0;
}

bool DisplayDevice::setBacklight(std::uint8_t) {
	return false;
}

bool DisplayDevice::fillRgb565(Rect const&, std::uint16_t) {
	return false;
}

bool DisplayDevice::beginRgb565Stream(Rect const&) {
	return false;
}

bool DisplayDevice::writeRgb565Stream(std::uint16_t const*, int) {
	return false;
}

bool DisplayDevice::writeRgb565BytesStream(std::uint8_t const*, int) {
	return false;
}

bool DisplayDevice::writeRgb888Stream(std::uint8_t const*, int) {
	return false;
}

bool DisplayDevice::endRgb565Stream() {
	return false;
}

bool DisplayDevice::configureVerticalScroll(int, int, int) {
	return false;
}

bool DisplayDevice::setVerticalScrollStart(int) {
	return false;
}

} // namespace lexxos::devices
