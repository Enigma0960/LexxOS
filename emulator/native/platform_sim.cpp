#include <lexxos/platform/platform.h>

#include <sim_runtime.h>

using lexxos::platform::BoardDescriptor;
using lexxos::platform::GameBoyRomFlashRegion;
using lexxos::platform::GpioActiveLevel;
using lexxos::platform::GpioInterruptEdge;
using lexxos::platform::GpioInterruptHandler;
using lexxos::platform::GpioPin;
using lexxos::platform::GpioPull;
using lexxos::platform::ProcessorFamily;

namespace {

lexxos::emulator::SimRuntime* runtime() {
	return lexxos::emulator::activeRuntime();
}

} // namespace

BoardDescriptor const& lexxos::platform::activeBoardDescriptor() {
	static BoardDescriptor const descriptor{
	    .boardId = "dev-sim",
	    .boardName = "LexxOS simulated board",
	    .vendor = "LexxOS",
	    .processorFamily = ProcessorFamily::NativeHost,
	    .processorName = "sim-cpu",
	    .statusLed = {.number = 25},
	    .debugUart = {.index = 0, .tx = {.number = 0}, .rx = {.number = 1}, .baudRate = 115200},
	    .debugOledI2c = {.index = 0, .sda = {.number = 20}, .scl = {.number = 21}, .baudRate = 400000},
	    .sdSpi = {.index = 0, .sck = {.number = 18}, .mosi = {.number = 19}, .miso = {.number = 16}, .cs = {.number = 17}, .baudRate = 24000000},
	    .sdCardDetect = {.detect = {.number = 22}, .insertedLevel = GpioActiveLevel::Low},
	};

	return descriptor;
}

void lexxos::platform::initialize() {
}

BoardDescriptor const& lexxos::platform::board() {
	return activeBoardDescriptor();
}

void lexxos::platform::writeConsole(std::string_view text) {
	if (auto* active = runtime(); active != nullptr) {
		active->writeConsole(text);
	}
}

bool lexxos::platform::readConsoleChar(char& value) {
	if (auto* active = runtime(); active != nullptr) {
		return active->readConsoleChar(value);
	}
	return false;
}

std::size_t lexxos::platform::readConsoleAvailable(char* buffer, std::size_t capacity) {
	if (buffer == nullptr || capacity == 0) {
		return 0;
	}

	auto* active = runtime();
	if (active == nullptr) {
		return 0;
	}

	std::size_t count = 0;
	while (count < capacity && active->readConsoleChar(buffer[count])) {
		++count;
	}
	return count;
}

void lexxos::platform::sleepMs(std::uint32_t milliseconds) {
	if (auto* active = runtime(); active != nullptr) {
		active->sleepMs(milliseconds);
	}
}

std::uint32_t lexxos::platform::uptimeMs() {
	if (auto* active = runtime(); active != nullptr) {
		return active->uptimeMs();
	}
	return 0;
}

lexxos::platform::PlatformPerformanceSnapshot lexxos::platform::performanceSnapshot() {
	return {};
}

void lexxos::platform::setStatusLed(bool enabled) {
	if (auto* active = runtime(); active != nullptr) {
		active->gpioWrite(static_cast<std::uint8_t>(board().statusLed.number), enabled);
	}
}

bool lexxos::platform::configureGpioInput(GpioPin pin, GpioPull) {
	return pin.isValid();
}

bool lexxos::platform::configureGpioOutput(GpioPin pin, bool initialValue) {
	if (!pin.isValid()) {
		return false;
	}

	if (auto* active = runtime(); active != nullptr) {
		active->gpioWrite(static_cast<std::uint8_t>(pin.number), initialValue);
		return true;
	}
	return false;
}

bool lexxos::platform::readGpio(GpioPin pin, bool& value) {
	if (!pin.isValid()) {
		return false;
	}

	if (auto* active = runtime(); active != nullptr) {
		return active->gpioRead(static_cast<std::uint8_t>(pin.number), value);
	}
	return false;
}

bool lexxos::platform::writeGpio(GpioPin pin, bool value) {
	if (!pin.isValid()) {
		return false;
	}

	if (auto* active = runtime(); active != nullptr) {
		active->gpioWrite(static_cast<std::uint8_t>(pin.number), value);
		return true;
	}
	return false;
}

bool lexxos::platform::configureGpioInterrupt(GpioPin pin, GpioInterruptEdge edge, GpioInterruptHandler handler, void* userData) {
	if (!pin.isValid()) {
		return false;
	}

	if (auto* active = runtime(); active != nullptr) {
		return active->configureGpioInterrupt(static_cast<std::uint8_t>(pin.number), edge, handler, userData);
	}
	return false;
}

void lexxos::platform::disableGpioInterrupt(GpioPin pin) {
	if (!pin.isValid()) {
		return;
	}

	if (auto* active = runtime(); active != nullptr) {
		active->disableGpioInterrupt(static_cast<std::uint8_t>(pin.number));
	}
}

bool lexxos::platform::gameBoyRomFlashRegion(GameBoyRomFlashRegion& output) {
	output = {};
	return false;
}

bool lexxos::platform::eraseAndProgramGameBoyRomFlash(std::uint32_t, void const*, std::size_t) {
	return false;
}

void lexxos::platform::reboot() {
	writeConsole("reboot requested in simulator\r\n");
	if (auto* active = runtime(); active != nullptr) {
		active->requestShutdown();
	}
}
