#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <blink_application.h>
#include <lexxos/core/core_system.h>
#include <lexxos/platform/platform_services.h>

namespace {

void expect(bool value, char const* message) {
	if (!value) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}

std::string console;
std::vector<bool> levels;
std::vector<std::uint32_t> delays;
unsigned starts{}, configures{};
bool configureOk{true}, writeOk{true};
lexxos::platform::LegacyBoardDescriptor descriptor{
    .statusLed = {2},
    .statusLedActiveLevel = lexxos::platform::GpioActiveLevel::High,
    .userGpio = {.pins = {{.pin = {2}, .label = "led", .inputAllowed = false, .driver = lexxos::platform::GpioDriver::DigitalOutput}}, .count = 1},
};

lexxos::platform::LegacyBoardDescriptor const& currentBoard() {
	return descriptor;
}

void initialize() {
	++starts;
}

void writeConsole(std::string_view text) {
	console += text;
}

void sleep(std::uint32_t ms) {
	delays.push_back(ms);
}

bool configureOutput(lexxos::platform::GpioPin pin, bool value) {
	expect(pin.number == 2, "incorrect GPIO");
	++configures;
	if (configureOk) {
		levels.push_back(value);
	}

	return configureOk;
}

bool writePin(lexxos::platform::GpioPin pin, bool value) {
	expect(pin.number == 2, "incorrect GPIO write");
	if (writeOk) {
		levels.push_back(value);
	}

	return writeOk;
}

lexxos::platform::PlatformServices const platform{
    .lifecycle = {.initialize = initialize},
    .board = {.current = currentBoard},
    .console = {.write = writeConsole},
    .clock = {.sleepMs = sleep},
    .gpio = {.configureOutput = configureOutput, .write = writePin},
};

void reset() {
	console.clear();
	levels.clear();
	delays.clear();
	starts = configures = 0;
	configureOk = writeOk = true;
}

} // namespace

namespace lexxos::platform {

LegacyBoardDescriptor const& board() {
	expect(false, "global board fallback");
	return descriptor;
}

void sleepMs(std::uint32_t) {
	expect(false, "global sleep fallback");
}

void writeConsole(std::string_view) {
	expect(false, "global console fallback");
}

} // namespace lexxos::platform

int main() {
	for (bool activeHigh : {true, false}) {
		reset();
		descriptor.statusLedActiveLevel = activeHigh ? lexxos::platform::GpioActiveLevel::High : lexxos::platform::GpioActiveLevel::Low;
		descriptor.userGpio.pins[0].initialHigh = !activeHigh;
		lexxos::lifecycle::EventBus events;
		lexxos::core::CoreSystem system{platform, events};
		lexxos::ce::blink::BlinkApplication app{system};
		expect(!app.step() && delays.empty() && levels.empty(), "step before initialization");
		expect(app.initialize(), "initialization failed");
		expect(starts == 1 && configures == 1 && levels == std::vector<bool>{!activeHigh}, "LED must start OFF");
		expect(system.devices().gpioPin("led") != nullptr, "LED not registered");
		expect(console.find("LexxCE:") < console.find("initialization complete"), "boot log order");
		expect(console.find("GPIO2: OFF") != std::string::npos, "initial LED state not logged");
		expect(app.initialize() && starts == 1 && configures == 1, "double initialization reset hardware");
		console.clear();
		expect(app.step() && app.step() && app.step(), "blink step failed");
		expect(delays == std::vector<std::uint32_t>{1000, 1000, 1000}, "wrong blink interval");
		expect(levels == std::vector<bool>{!activeHigh, activeHigh, !activeHigh, activeHigh}, "wrong LED levels/polarity");
		auto on = console.find("GPIO2: ON");
		auto off = console.find("GPIO2: OFF", on);
		expect(on < off && off < console.find("GPIO2: ON", off), "missing transition logs");
		writeOk = false;
		console.clear();
		expect(!app.step(), "GPIO failure ignored");
		expect(console.find("LED state unknown") != std::string::npos && console.find("GPIO2: OFF") == std::string::npos,
		    "failed write reported as successful state");
		auto delayCount = delays.size();
		expect(!app.step() && delays.size() == delayCount && levels.size() == 4, "failed app continued toggling");
	}

	reset();
	configureOk = false;
	lexxos::lifecycle::EventBus events;
	lexxos::core::CoreSystem system{platform, events};
	lexxos::ce::blink::BlinkApplication app{system};
	expect(!app.initialize() && !app.step(), "GPIO configuration failure ignored");
	expect(system.devices().gpioPinCount() == 0 && levels.empty(), "failed output registered as working");
	expect(console.find("[error]") != std::string::npos && console.find("initialization complete") == std::string::npos,
	    "failed startup reported as ready");
}
