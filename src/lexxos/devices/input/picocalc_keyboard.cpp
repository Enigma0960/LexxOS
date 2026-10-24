#include <lexxos/devices/input/picocalc_keyboard.h>

namespace lexxos::devices {

PicoCalcKeyboard::PicoCalcKeyboard(
    I2cBus& bus,
    lexxos::platform::PlatformServices const& platform,
    std::uint8_t address)
    : _controller(bus, platform, address) {
}

bool PicoCalcKeyboard::initialize() {
	return _controller.initialize();
}

void PicoCalcKeyboard::update() {
	_controller.update();
}

void PicoCalcKeyboard::setRawEventReporting(bool enabled) {
	_controller.setRawEventReporting(enabled);
}

std::string_view PicoCalcKeyboard::name() const {
	return _controller.name();
}

bool PicoCalcKeyboard::isReady() const {
	return _controller.isReady();
}

bool PicoCalcKeyboard::pollEvent(InputEvent& event) {
	return _controller.pollEvent(event);
}

bool PicoCalcKeyboard::isPressed(KeyCode key) const {
	return _controller.isPressed(key);
}

PicoCalcSouthbridgeRuntimeService& PicoCalcKeyboard::southbridgeRuntimeService() {
	return _controller.runtimeService();
}

PicoCalcSouthbridgeRuntimeService const& PicoCalcKeyboard::southbridgeRuntimeService() const {
	return _controller.runtimeService();
}

std::uint32_t PicoCalcKeyboard::rawState() const {
	return _controller.rawState();
}

std::uint32_t PicoCalcKeyboard::rawKeyCount() const {
	return _controller.rawKeyCount();
}

std::uint32_t PicoCalcKeyboard::rawRowCount() const {
	return _controller.rawRowCount();
}

std::uint32_t PicoCalcKeyboard::rawColumnCount() const {
	return _controller.rawColumnCount();
}

std::uint32_t PicoCalcKeyboard::staleScanCodeReleases() const {
	return _controller.staleScanCodeReleases();
}

std::uint32_t PicoCalcKeyboard::staleReleaseCount() const {
	return _controller.staleReleaseCount();
}

} // namespace lexxos::devices
