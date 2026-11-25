#include <cstdlib>
#include <iostream>
#include <vector>

#include <lexxos/devices/input/digital_input.h>

namespace {

using Event = lexxos::devices::DigitalInputEvent;
using lexxos::devices::DigitalInput;
using lexxos::devices::DigitalInputConfig;
using lexxos::platform::GpioActiveLevel;
using lexxos::platform::GpioPin;
using lexxos::platform::GpioPull;

bool level = true;
bool readOk = true;
bool configureOk = true;
GpioPull expectedPull = GpioPull::Up;
std::vector<Event> events;
std::vector<bool> eventLevels;

void check(bool passed, char const* reason) {
	if (!passed) {
		std::cerr << reason << '\n';
		std::exit(1);
	}
}

lexxos::platform::PlatformGpioService const gpio{
    .configureInput = [](GpioPin pin, GpioPull pull) { return configureOk && pin.number == 27 && pull == expectedPull; },
    .read = [](GpioPin pin, bool& value) { value = level; return readOk && pin.number == 27; },
};

void receive(Event event, bool high, void* context) {
	check(context == &events, "context");
	events.push_back(event);
	eventLevels.push_back(high);
}

void expect(std::initializer_list<Event> expected) {
	check(events == std::vector<Event>(expected), "incorrect event sequence");
	events.clear();
	eventLevels.clear();
}

void sample(DigitalInput& input, std::uint32_t time, bool high) {
	level = high;
	check(input.poll(time), "poll failed");
}

void edge(DigitalInput& input, std::uint32_t time, bool high) {
	sample(input, time, high);
	sample(input, time + 25, high);
}

void tap(DigitalInput& input, std::uint32_t time) {
	edge(input, time, false);
	check(eventLevels == std::vector<bool>({false, false}), "falling level");
	expect({Event::Changed, Event::BecameLow});
	edge(input, time + 60, true);
	check(input.high() && !input.pressed(), "stable high after release");
}

void testClickSeries(DigitalInput& input) {
	for (unsigned count = 1; count <= 3; ++count) {
		check(input.initialize(receive, &events), "initialize");
		for (unsigned i = 0; i < count; ++i) {
			tap(input, 100 + i * 120);
			if (i == 2) {
				expect({Event::Changed, Event::BecameHigh, Event::TripleClick});
			} else {
				check(eventLevels == std::vector<bool>({true, true}), "rising level");
				expect({Event::Changed, Event::BecameHigh});
			}
		}

		auto const release = 160 + (count - 1) * 120;
		sample(input, release + 299, true);
		expect({});
		sample(input, release + 300, true);
		if (count == 1) {
			expect({Event::SingleClick});
		}

		if (count == 2) {
			expect({Event::DoubleClick});
		}

		if (count == 3) {
			expect({});
		}
	}
}

void testHoldAndDebounce(DigitalInput& input) {
	check(input.initialize(receive, &events), "initialize");
	sample(input, 100, false);
	sample(input, 124, true);
	check(input.high(), "brief pulse must not change stable input");
	expect({});
	edge(input, 200, false);
	expect({Event::Changed, Event::BecameLow});
	sample(input, 1699, false);
	expect({});
	sample(input, 1700, false);
	expect({Event::Hold});
	sample(input, 10199, false);
	expect({});
	sample(input, 10200, false);
	sample(input, 11000, false);
	expect({Event::LongHold});
	edge(input, 12000, true);
	sample(input, 12500, true);
	expect({Event::Changed, Event::BecameHigh});

	// Контакт отпущен до порога. Подтверждение отпускания пересекает порог.
	edge(input, 13000, false);
	edge(input, 14499, true);
	sample(input, 14800, true);
	expect({Event::Changed, Event::BecameLow, Event::Changed, Event::BecameHigh, Event::SingleClick});
}

void testClickBoundary(DigitalInput& input) {
	check(input.initialize(receive, &events), "initialize");
	edge(input, 100, false);
	edge(input, 160, true);
	edge(input, 459, false); // Второе нажатие за 1 мс до конца окна.
	edge(input, 530, true);
	sample(input, 830, true);
	expect({Event::Changed, Event::BecameLow, Event::Changed, Event::BecameHigh,
	    Event::Changed, Event::BecameLow, Event::Changed, Event::BecameHigh, Event::DoubleClick});
	edge(input, 900, false);
	edge(input, 980, true);
	edge(input, 1280, false); // Ровно на границе начинается новая серия.
	sample(input, 2780, false);
	expect({Event::Changed, Event::BecameLow, Event::Changed, Event::BecameHigh,
	    Event::SingleClick, Event::Changed, Event::BecameLow, Event::Hold});
	edge(input, 2800, true);
	expect({Event::Changed, Event::BecameHigh});
}

void testClickThenHold(DigitalInput& input) {
	check(input.initialize(receive, &events), "initialize");
	tap(input, 100);
	expect({Event::Changed, Event::BecameHigh});
	edge(input, 200, false);
	expect({Event::Changed, Event::BecameLow});
	// Следующий опрос сразу после обоих порогов.
	sample(input, 10200, false);
	expect({Event::SingleClick, Event::Hold, Event::LongHold});
	edge(input, 10500, true);
	sample(input, 11000, true);
	expect({Event::Changed, Event::BecameHigh});
}

void testStartupErrorsAndWrap() {
	DigitalInputConfig config{.pin = {27}, .pull = GpioPull::Down, .activeLevel = GpioActiveLevel::High, .debounceMs = 5, .clickGapMs = 15, .holdMs = 50, .longHoldMs = 100};
	expectedPull = config.pull;
	DigitalInput input(config, gpio);
	level = true;
	check(input.initialize(receive, &events), "initialize held high");
	sample(input, 100000, true);
	expect({});
	edge(input, 100100, false);
	expect({Event::Changed, Event::BecameLow});
	sample(input, UINT32_MAX - 2, true);
	sample(input, 2, true);
	expect({Event::Changed, Event::BecameHigh});
	sample(input, 47, true);
	sample(input, 97, true);
	expect({Event::Hold, Event::LongHold});

	readOk = false;
	check(!input.poll(100), "read failure");
	readOk = true;
	check(!input.poll(101), "must reinitialize after read failure");
	configureOk = false;
	check(!input.initialize(receive, &events), "configure failure");
	configureOk = true;
	check(input.initialize(receive, &events), "retry");
	sample(input, 1000, true);
	expect({});
	config.longHoldMs = config.holdMs;
	DigitalInput invalid(config, gpio);
	check(!invalid.initialize(receive, &events), "invalid thresholds");
}

} // namespace

int main() {
	DigitalInput input({.pin = {27}, .pull = GpioPull::Up}, gpio);
	testClickSeries(input);
	testHoldAndDebounce(input);
	testClickBoundary(input);
	testClickThenHold(input);
	testStartupErrorsAndWrap();
}
