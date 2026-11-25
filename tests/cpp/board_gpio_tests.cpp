#include <array>
#include <cstdlib>
#include <iostream>

#include <lexxos/devices/device_manager.h>

namespace {

using Pin = lexxos::platform::BoardUserGpioPin;
using lexxos::devices::DeviceManager;
using lexxos::devices::DigitalInputEvent;
using lexxos::devices::DigitalOutputState;
using lexxos::platform::BoardUserGpio;
using lexxos::platform::GpioActiveLevel;
using lexxos::platform::GpioPin;
using lexxos::platform::GpioPull;

constexpr lexxos::platform::GpioInputTiming timing{.debounceMs = 5};
constexpr BoardUserGpio board{
    .pins = {
             Pin::digitalInput({0}, "in1", GpioPull::Down, GpioActiveLevel::High, &timing),
             Pin::digitalOutput({4}, "out1", true),
             {{8}, "raw", true, true},
             Pin::digitalInput({1}, "in2"),
             Pin::digitalOutput({5}, "out2"),
             Pin::digitalInput({2}, "in3"),
             Pin::digitalOutput({6}, "out3"),
             Pin::digitalInput({3}, "in4"),
             Pin::digitalOutput({7}, "out4"),
             },
    .count = 9,
};

std::array<bool, 9> levels{};
std::array<unsigned, 9> reads{};
unsigned configurations{};
int badPin = -1;

void expect(bool ok, char const* message) {
	if (!ok) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}

lexxos::platform::PlatformGpioService const gpio{
    .configureInput = [](GpioPin pin, GpioPull pull) {
	    ++configurations;
	    return pin.number < 4 && pull == (pin.number == 0 ? GpioPull::Down : GpioPull::None); },
    .configureOutput = [](GpioPin pin, bool high) {
	    ++configurations;
	    levels.at(pin.number) = high;
	    return pin.number != badPin; },
    .read = [](GpioPin pin, bool& value) {
	    ++reads.at(pin.number);
	    value = levels.at(pin.number);
	    return pin.number != badPin; },
    .write = [](GpioPin pin, bool high) {
	    levels.at(pin.number) = high;
	    return true; },
};

void changed(DigitalInputEvent event, bool high, void* context) {
	if (event == DigitalInputEvent::Changed && high) {
		++*static_cast<unsigned*>(context);
	}
}

} // namespace

int main() {
	DeviceManager devices;
	devices.initialize();
	expect(devices.bindBoardGpio(board, gpio) && configurations == 8, "4 inputs + 4 outputs");
	expect(devices.gpioPinCount() == 8 && !devices.digitalInput("raw"), "unassigned GPIO");
	auto* output = devices.digitalOutput("out1");
	expect(output && output == devices.digitalOutput("out1") && output->state() == DigitalOutputState::High, "shared output");
	expect(output->setLow() && !levels[4], "output control");
	expect(!devices.digitalInput("out1") && !devices.digitalOutput("in4") && !devices.digitalOutput("missing"), "typed lookup");
	auto* input = devices.digitalInput("in1");
	expect(input && input == devices.digitalInput("in1") && !input->pressed(), "shared input");
	unsigned changes = 0;
	input->setHandler(changed, &changes);
	levels[0] = true;
	expect(devices.pollDigitalInputs(100) && devices.pollDigitalInputs(104) && !input->high(), "custom debounce");
	expect(devices.pollDigitalInputs(105) && input->pressed() && changes == 1, "input event");
	for (int i = 0; i < 4; ++i) {
		expect(reads[i] == 4, "each input sampled once per poll");
	}

	badPin = 1;
	expect(!devices.pollDigitalInputs(110) && reads[3] == 5, "read failure must not starve later inputs");
	badPin = -1;
	expect(devices.digitalInput("in2")->initialize() && devices.pollDigitalInputs(115), "input retry");
	expect(!devices.bindBoardGpio(board, gpio) && devices.digitalOutput("out1") == output, "duplicate bind");
	devices.initialize();
	expect(!devices.digitalOutput("out1") && !devices.digitalInput("in1"), "registry reset");

	auto invalid = board;
	invalid.pins[8].label = "in1";
	auto const before = configurations;
	expect(!devices.bindBoardGpio(invalid, gpio), "duplicate name");
	invalid = board;
	invalid.pins[2].pin = {0};
	expect(!devices.bindBoardGpio(invalid, gpio), "raw pin alias");
	invalid = board;
	invalid.pins[0].inputAllowed = false;
	expect(!devices.bindBoardGpio(invalid, gpio), "invalid direction");
	invalid.count = 17;
	expect(!devices.bindBoardGpio(invalid, gpio) && configurations == before, "validate before configuring");
	badPin = 7;
	expect(!devices.bindBoardGpio(board, gpio) && !devices.digitalOutput("out1") && devices.gpioPinCount() == 0, "partial bind");
	badPin = -1;
	expect(devices.bindBoardGpio(board, gpio) && levels[4], "retry after partial failure");
}
