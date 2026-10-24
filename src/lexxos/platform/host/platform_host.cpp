#include <lexxos/platform/platform.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

#if defined(__unix__) || defined(__APPLE__)
	#include <cerrno>
	#include <poll.h>
	#include <unistd.h>
#elif defined(_WIN32)
	#include <io.h>
	#include <cstdio>
#endif

using lexxos::platform::BoardDescriptor;
using lexxos::platform::GameBoyRomFlashRegion;
using lexxos::platform::GpioInterruptEdge;
using lexxos::platform::GpioInterruptHandler;
using lexxos::platform::GpioPin;
using lexxos::platform::GpioPull;
using lexxos::platform::ProcessorFamily;

namespace {

using Clock = std::chrono::steady_clock;

Clock::time_point g_startTime;

} // namespace

BoardDescriptor const& lexxos::platform::activeBoardDescriptor() {
	static BoardDescriptor const descriptor{
	    .boardId = "host_native",
	    .boardName = "Native host simulation",
	    .vendor = "LexxOS",
	    .processorFamily = ProcessorFamily::NativeHost,
	    .processorName = "host-cpu",
	};

	return descriptor;
}

void lexxos::platform::initialize() {
	g_startTime = Clock::now();
}

BoardDescriptor const& lexxos::platform::board() {
	return activeBoardDescriptor();
}

void lexxos::platform::writeConsole(std::string_view text) {
	std::cout.write(text.data(), static_cast<std::streamsize>(text.size()));
	std::cout.flush();
}

bool lexxos::platform::isConsoleInteractive() {
#if defined(__unix__) || defined(__APPLE__)
	return ::isatty(STDIN_FILENO) != 0;
#elif defined(_WIN32)
	return ::_isatty(::_fileno(stdin)) != 0;
#else
	return true;
#endif
}

bool lexxos::platform::readConsoleChar(char& value) {
#if defined(__unix__) || defined(__APPLE__)
	ssize_t count;
	do {
		count = ::read(STDIN_FILENO, &value, 1);
	} while (count < 0 && errno == EINTR);
	return count == 1;
#else
	int const next = std::cin.get();
	if (next == EOF) {
		return false;
	}

	value = static_cast<char>(next);
	return true;
#endif
}

std::size_t lexxos::platform::readConsoleAvailable(char* buffer, std::size_t capacity) {
	if (buffer == nullptr || capacity == 0) {
		return 0;
	}

#if defined(__unix__) || defined(__APPLE__)
	// streambuf::in_avail() need not report bytes waiting in a terminal or pipe.
	// Use the descriptor consistently with readConsoleChar, without stdio buffering.
	pollfd input{STDIN_FILENO, POLLIN, 0};
	int ready;
	do {
		ready = ::poll(&input, 1, 0);
	} while (ready < 0 && errno == EINTR);
	if (ready <= 0 || (input.revents & (POLLIN | POLLHUP)) == 0) {
		return 0;
	}
	ssize_t count;
	do {
		count = ::read(STDIN_FILENO, buffer, capacity);
	} while (count < 0 && errno == EINTR);
	return count > 0 ? static_cast<std::size_t>(count) : 0;
#else
	auto available = std::cin.rdbuf()->in_avail();
	if (available <= 0) {
		return 0;
	}

	std::size_t count = 0;
	while (count < capacity && available > 0) {
		int const next = std::cin.get();
		if (next == EOF) {
			break;
		}
		buffer[count++] = static_cast<char>(next);
		--available;
	}
	return count;
#endif
}

void lexxos::platform::sleepMs(std::uint32_t milliseconds) {
	std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

std::uint32_t lexxos::platform::uptimeMs() {
	auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - g_startTime);
	return static_cast<std::uint32_t>(elapsed.count());
}

lexxos::platform::PlatformPerformanceSnapshot lexxos::platform::performanceSnapshot() {
	return {};
}

void lexxos::platform::setStatusLed(bool) {
}

bool lexxos::platform::configureGpioInput(GpioPin pin, GpioPull) {
	return pin.isValid();
}

bool lexxos::platform::configureGpioOutput(GpioPin pin, bool) {
	return pin.isValid();
}

bool lexxos::platform::readGpio(GpioPin pin, bool& value) {
	if (!pin.isValid()) {
		return false;
	}

	value = false;
	return true;
}

bool lexxos::platform::writeGpio(GpioPin pin, bool) {
	return pin.isValid();
}

bool lexxos::platform::configureGpioInterrupt(GpioPin pin, GpioInterruptEdge, GpioInterruptHandler handler, void*) {
	return pin.isValid() && handler != nullptr && false;
}

void lexxos::platform::disableGpioInterrupt(GpioPin) {
}

bool lexxos::platform::gameBoyRomFlashRegion(GameBoyRomFlashRegion& output) {
	output = {};
	return false;
}

bool lexxos::platform::eraseAndProgramGameBoyRomFlash(std::uint32_t, void const*, std::size_t) {
	return false;
}

void lexxos::platform::reboot() {
	writeConsole("reboot is not available on host platform\r\n");
}
