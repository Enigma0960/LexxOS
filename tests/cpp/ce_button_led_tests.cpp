#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <board.h>

extern "C" void app_main();

namespace {

using lexxos::platform::BoardUserGpioPin;
using lexxos::platform::GpioPin;
using lexxos::platform::GpioPull;

std::uint32_t now{};
std::string logText;
std::vector<bool> levels;

void expect(bool ok, char const* reason) {
	if (!ok) {
		std::cerr << reason << '\n'
		          << logText;
		std::exit(1);
	}
}

lexxos::platform::LegacyBoardDescriptor const& testBoard() {
	static constexpr lexxos::platform::LegacyBoardDescriptor board{
	    .userGpio = {.pins = {BoardUserGpioPin::digitalInput({27}, "button", GpioPull::Up),
	                     BoardUserGpioPin::digitalOutput({2}, "led")},
	                 .count = 2}
    };
	return board;
}

bool readButton(GpioPin pin, bool& high) {
	expect(pin.number == 27, "wrong input");
	// Кнопка зажата при загрузке. Затем идут один, два, три клика и одно удержание.
	constexpr unsigned presses[][2] = {
	    {   0,   100},
	    { 200,   260},
	    { 800,   860},
	    { 950,  1010},
	    {1500,  1560},
	    {1630,  1690},
	    {1760,  1820},
	    {2300, 12500}
    };
	high = true;
	for (auto const& interval : presses) {
		if (now >= interval[0] && now < interval[1]) {
			high = false;
		}
	}

	return now < 13000;
}

bool writeLed(GpioPin pin, bool high) {
	expect(pin.number == 2, "wrong output");
	levels.push_back(high);
	return true;
}

} // namespace

lexxos::platform::PlatformServices const& button_led::platform() {
	static lexxos::platform::PlatformServices const services{
	    .board = {.current = testBoard},
	    .console = {.write = [](std::string_view line) { logText += line; }},
	    .clock = {.sleepMs = [](std::uint32_t ms) { expect(ms == 5, "poll interval"); now += ms; },
	              .uptimeMs = [] { return now; }},
	    .gpio = {.configureInput = [](GpioPin pin, GpioPull pull) { return pin.number == 27 && pull == GpioPull::Up; },
	              .configureOutput = writeLed,
	              .read = readButton,
	              .write = writeLed},
	};
	return services;
}

namespace lexxos::platform {

LegacyBoardDescriptor const& board() {
	return testBoard();
}

void sleepMs(std::uint32_t) {
	expect(false, "unexpected fallback clock");
}

void writeConsole(std::string_view) {
	expect(false, "unexpected fallback console");
}

} // namespace lexxos::platform

int main() {
	app_main();
	expect(levels == std::vector<bool>{false, true, true, false}, "unexpected LED writes");
	for (auto line : {"[0 ms] kernel ready button=0 led=OFF", "[125 ms] released button=1 led=OFF",
	         "[225 ms] pressed button=0 led=OFF", "[560 ms] single button=1 led=ON",
	         "[1310 ms] double button=1 led=ON", "[1845 ms] triple button=1 led=OFF",
	         "[3800 ms] hold button=0 led=OFF", "[12300 ms] long hold button=0 led=OFF",
	         "[error] GPIO read failed"}) {
		expect(logText.find(line) != std::string::npos, line);
	}
}
