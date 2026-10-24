#include <lexxos/devices/debug_display.h>

#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/display/ssd1306_display.h>
#include <lexxos/platform/platform_services.h>

namespace {

lexxos::devices::Ssd1306TextDisplay& debugDisplay() {
	static lexxos::devices::Ssd1306TextDisplay display{lexxos::devices::debugI2cBus()};
	return display;
}

lexxos::platform::BoardDescriptor const& boardFor(lexxos::platform::PlatformServices const& platform) {
	auto const& board = platform.board;
	return board.current != nullptr ? board.current() : lexxos::platform::board();
}

void sleepMs(lexxos::platform::PlatformServices const& platform, std::uint32_t milliseconds) {
	auto const& clock = platform.clock;
	if (clock.sleepMs != nullptr) {
		clock.sleepMs(milliseconds);
	}
}

} // namespace

bool lexxos::devices::debug_display::initialize() {
	return initialize(lexxos::platform::services());
}

bool lexxos::devices::debug_display::initialize(lexxos::platform::PlatformServices const& platform) {
	auto const& descriptor = boardFor(platform);
	if (!descriptor.debugOledI2c.isValid()) {
		return false;
	}

	auto& bus = debugI2cBus();
	if (!bus.isReady() && !bus.initialize(descriptor.debugOledI2c)) {
		return false;
	}

	sleepMs(platform, 50);
	return debugDisplay().initialize();
}

bool lexxos::devices::debug_display::isReady() {
	return debugDisplay().isReady();
}

void lexxos::devices::debug_display::clear() {
	debugDisplay().clear();
}

void lexxos::devices::debug_display::writeLine(std::uint8_t line, std::string_view text) {
	debugDisplay().writeLine(line, text);
}
