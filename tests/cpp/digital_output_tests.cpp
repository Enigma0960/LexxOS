#include <cstdlib>
#include <iostream>
#include <vector>

#include <lexxos/devices/output/digital_output.h>

namespace {

using lexxos::devices::DigitalOutput;
using State = lexxos::devices::DigitalOutputState;
using lexxos::platform::GpioPin;
using lexxos::platform::PlatformGpioService;

std::vector<bool> writes;
std::vector<bool> initialLevels;
bool configureOk = true;
bool writeOk = true;

void check(bool valid, char const* message) {
	if (!valid) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}

bool configure(GpioPin pin, bool high) {
	check(pin.number == 2, "configuration pin");
	initialLevels.push_back(high);
	return configureOk;
}

bool write(GpioPin pin, bool high) {
	check(pin.number == 2, "write pin");
	writes.push_back(high);
	return writeOk;
}

} // namespace

int main() {
	PlatformGpioService gpio{.configureOutput = configure, .write = write};
	DigitalOutput output(gpio);
	check(output.state() == State::Unknown && !output.toggle() && !output.setHigh(), "before initialization");
	check(!output.initialize({-1}) && initialLevels.empty(), "invalid pin");
	check(output.initialize({2}) && output.state() == State::Low && output.pin().number == 2, "initial low");
	check(output.toggle() && output.state() == State::High, "toggle high");
	check(output.toggle() && output.state() == State::Low, "toggle low");
	check(output.setHigh() && output.state() == State::High, "set high");
	check(output.write(true) && output.state() == State::High, "reassert high");
	check(output.setLow() && output.state() == State::Low, "set low");
	check(writes == std::vector<bool>{true, false, true, true, false}, "GPIO levels");

	writeOk = false;
	check(!output.toggle() && output.state() == State::Unknown, "failed write");
	auto const attempts = writes.size();
	check(!output.toggle() && writes.size() == attempts, "unknown state toggle");
	writeOk = true;
	check(output.setHigh() && output.state() == State::High && output.toggle() && output.state() == State::Low, "recover");
	check(output.initialize({2}, true) && output.state() == State::High, "initial high");

	configureOk = false;
	check(!output.initialize({2}) && !output.pin().isValid() && output.state() == State::Unknown, "failed configure");
	check(!output.setLow(), "failed configure must disable writes");
	configureOk = true;
	check(output.initialize({2}), "retry configure");
	gpio.write = nullptr;
	check(!output.initialize({2}), "missing write");
	gpio.write = write;
	gpio.configureOutput = nullptr;
	check(!output.initialize({2}), "missing configure");
	check(initialLevels == std::vector<bool>{false, true, false, false}, "configuration attempts");
}
