#include <sim_display_device.h>

#include <sim_runtime.h>

namespace lexxos::emulator {

std::string_view SimDisplayDevice::name() const {
	return "emu_display";
}

bool SimDisplayDevice::isReady() const {
	return activeRuntime() != nullptr;
}

int SimDisplayDevice::width() const {
	auto* active = activeRuntime();
	return active != nullptr ? static_cast<int>(active->displayWidth()) : static_cast<int>(SimRuntime::defaultDisplayWidth);
}

int SimDisplayDevice::height() const {
	auto* active = activeRuntime();
	return active != nullptr ? static_cast<int>(active->displayHeight()) : static_cast<int>(SimRuntime::defaultDisplayHeight);
}

lexxos::devices::PixelFormat SimDisplayDevice::nativeFormat() const {
	return lexxos::devices::PixelFormat::Rgb565;
}

bool SimDisplayDevice::clear(std::uint16_t rgb565) {
	auto* active = activeRuntime();
	if (active == nullptr) {
		return false;
	}

	return active->displayClear(rgb565);
}

bool SimDisplayDevice::beginFrame() {
	return activeRuntime() != nullptr;
}

bool SimDisplayDevice::blitRgb565(lexxos::devices::Rect const& rect, std::uint16_t const* pixels, int stridePixels) {
	auto* active = activeRuntime();
	if (active == nullptr) {
		return false;
	}

	return active->displayBlitRgb565(rect, pixels, stridePixels);
}

bool SimDisplayDevice::endFrame() {
	auto* active = activeRuntime();
	if (active == nullptr) {
		return false;
	}

	active->displayCommitFrame();
	return true;
}

SimDisplayDevice& simDisplayDevice() {
	static SimDisplayDevice device;
	return device;
}

} // namespace lexxos::emulator
