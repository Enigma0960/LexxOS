#include "blink_application.h"

#include <cstdio>

#include <lexxos/core/bootstrap.h>
#include <lexxos/core/core_system.h>
#include <lexxos/platform/platform_services.h>

namespace lexxos::ce::blink {

BlinkApplication::BlinkApplication(lexxos::core::CoreSystem& system)
    : _system(system) {
}

bool BlinkApplication::initialize() {
	auto const& platform = _system.platform();
	if (_ready) {
		return true;
	}

	if (platform.console.write == nullptr) {
		return false;
	}

	platform.console.write("\r\nLexxCE: kernel loading\r\n");
	if (!lexxos::core::bootstrap(_system)) {
		platform.console.write("[error] board GPIO initialization failed\r\n");
		return false;
	}

	platform.console.write("[core] The platform, scheduler, device registry, and EventBus are ready.\r\n");
	if (platform.board.current == nullptr || platform.clock.sleepMs == nullptr) {
		platform.console.write("[error] no board or clock services present\r\n");
		return false;
	}

	auto const& board = platform.board.current();
	_activeHigh = board.statusLedActiveLevel == lexxos::platform::GpioActiveLevel::High;
	_led = _system.devices().digitalOutput("led");
	if (!_led) {
		platform.console.write("[error] missing board output: led\r\n");
		return false;
	}

	_ready = true;
	platform.console.write("[core] initialization complete; LED interval: 1000ms\r\n");
	reportState();
	return true;
}

bool BlinkApplication::step() {
	if (!_ready) {
		return false;
	}

	_system.platform().clock.sleepMs(1000);
	return tick();
}

bool BlinkApplication::tick() {
	if (!_ready) {
		return false;
	}

	auto const& platform = _system.platform();
	if (!_led->toggle()) {
		_ready = false;
		platform.console.write("[error] GPIO write failed; blinking stopped, LED state unknown\r\n");
		return false;
	}

	reportState();
	return true;
}

void BlinkApplication::reportState() const {
	auto const high = _led->state() == lexxos::devices::DigitalOutputState::High;
	char message[96]{};
	std::snprintf(message, sizeof(message), "[LED] GPIO%d: %s (level=%d)\r\n",
	    _led->pin().number, high == _activeHigh ? "ON" : "OFF", high ? 1 : 0);
	_system.platform().console.write(message);
}

} // namespace lexxos::ce::blink
