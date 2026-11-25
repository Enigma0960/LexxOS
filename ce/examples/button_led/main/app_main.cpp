#include <cstdio>

#include "board.h"

#include <lexxos/core/bootstrap.h>
#include <lexxos/core/core_system.h>

namespace {

using Event = lexxos::devices::DigitalInputEvent;
using Output = lexxos::devices::DigitalOutput;

void report(char const* event, bool inputHigh, Output const& led) {
	auto const& platform = button_led::platform();
	using State = lexxos::devices::DigitalOutputState;
	auto const level = led.state();
	char line[100]{};
	std::snprintf(line, sizeof(line), "[%lu ms] %s button=%d led=%s\r\n",
	    static_cast<unsigned long>(platform.clock.uptimeMs()), event, inputHigh,
	    level == State::Unknown ? "unknown" : (level == State::High ? "ON" : "OFF"));
	platform.console.write(line);
}

void handleButton(Event event, bool high, void* context) {
	auto& led = *static_cast<Output*>(context);
	switch (event) {
		case Event::Changed    : return; // Следующее событие даст направление этого же перехода.
		case Event::BecameHigh : report("released", high, led); break;
		case Event::BecameLow  : report("pressed", high, led); break;
		case Event::SingleClick: report(led.toggle() ? "single" : "single: write error", high, led); break;
		case Event::DoubleClick: report(led.setHigh() ? "double" : "double: write error", high, led); break;
		case Event::TripleClick: report(led.setLow() ? "triple" : "triple: write error", high, led); break;
		case Event::Hold       : report("hold", high, led); break;
		case Event::LongHold   : report("long hold", high, led); break;
	}
}

} // namespace

extern "C" void app_main() {
	auto const& platform = button_led::platform();
	static lexxos::core::CoreSystem system{platform};
	platform.console.write("LexxCE: kernel loading\r\n");
	if (!lexxos::core::bootstrap(system)) {
		platform.console.write("[error] board GPIO initialization failed\r\n");
		return;
	}

	auto& devices = system.devices();
	auto* button = devices.digitalInput("button");
	auto* led = devices.digitalOutput("led");
	if (!button || !led) {
		platform.console.write("[error] missing button or LED\r\n");
		return;
	}

	report("kernel ready", button->high(), *led);
	button->setHandler(handleButton, led);
	while (devices.pollDigitalInputs(platform.clock.uptimeMs())) {
		platform.clock.sleepMs(5);
	}

	platform.console.write("[error] GPIO read failed\r\n");
}
