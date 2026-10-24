#include <lexxos/app/console_input.h>

#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>

#include <lexxos/platform/platform_services.h>
#include <lexxos/runtime/runtime_profile.h>

namespace {

std::string_view g_platformInput{};
std::size_t g_platformInputOffset{0};

void expect(bool condition, char const* message) {
	if (!condition) {
		std::cerr << "console input test failed: " << message << '\n';
		std::exit(1);
	}
}

bool readPlatformChar(char& value) {
	if (g_platformInputOffset >= g_platformInput.size()) {
		return false;
	}
	value = g_platformInput[g_platformInputOffset++];
	return true;
}

std::size_t readPlatformAvailable(char* buffer, std::size_t capacity) {
	if (buffer == nullptr) {
		return 0;
	}

	std::size_t count = 0;
	while (count < capacity && readPlatformChar(buffer[count])) {
		++count;
	}
	return count;
}

lexxos::platform::PlatformServices testPlatform() {
	return {
	    .console = {
	                .readChar = readPlatformChar,
	                .readAvailable = readPlatformAvailable,
	                },
	};
}

void resetPlatformInput(std::string_view text) {
	g_platformInput = text;
	g_platformInputOffset = 0;
	lexxos::app::console_input::clearInjected();
}

void expectRead(lexxos::platform::PlatformServices const& platform, char expected) {
	char value{};
	expect(lexxos::app::console_input::readChar(platform, value), "expected a readable character");
	expect(value == expected, "unexpected character");
}

void checkInjectedQueuePrecedesPlatformInput() {
	auto const platform = testPlatform();
	resetPlatformInput("xy");
	expect(lexxos::app::console_input::inject("ab"), "inject should accept a short sequence");

	expectRead(platform, 'a');
	expectRead(platform, 'b');
	expectRead(platform, 'x');
	expectRead(platform, 'y');

	char value{};
	expect(!lexxos::app::console_input::readChar(platform, value), "input should be empty after injected and platform bytes");
}

void checkInjectedSequenceIsAtomic() {
	auto const platform = testPlatform();
	resetPlatformInput("");

	std::array<char, lexxos::runtime::standardRuntimeProfile.shell.injectedInputBytes> large{};
	for (char& value : large) {
		value = 'z';
	}
	expect(!lexxos::app::console_input::inject({large.data(), large.size()}), "oversized injected sequence should fail");
	expect(lexxos::app::console_input::pendingInjected() == 0, "failed sequence must not partially enqueue");

	char value{};
	expect(!lexxos::app::console_input::readChar(platform, value), "failed sequence should leave input empty");
}

void checkReadAvailableDrainsPlatformInputBatch() {
	auto const platform = testPlatform();
	constexpr std::string_view pasted{"echo 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"};
	resetPlatformInput(pasted);

	std::array<char, 96> buffer{};
	auto const count = lexxos::app::console_input::readAvailable(platform, buffer.data(), buffer.size());

	expect(count == pasted.size(), "readAvailable should drain a full paste-sized platform batch");
	expect(std::string_view(buffer.data(), count) == pasted, "readAvailable should preserve pasted byte order");
}

void checkReadAvailableHonorsOutputCapacity() {
	auto const platform = testPlatform();
	resetPlatformInput("abcdef");

	std::array<char, 3> buffer{};
	auto const count = lexxos::app::console_input::readAvailable(platform, buffer.data(), buffer.size());

	expect(count == buffer.size(), "readAvailable should stop at caller capacity");
	expect(std::string_view(buffer.data(), count) == "abc", "readAvailable should fill the caller buffer first");
}

void checkModeState() {
	lexxos::app::console_input::setMode(lexxos::app::console_input::Mode::Program);
	expect(lexxos::app::console_input::mode() == lexxos::app::console_input::Mode::Program, "program mode should stick");
	lexxos::app::console_input::setMode(lexxos::app::console_input::Mode::Shell);
	expect(lexxos::app::console_input::mode() == lexxos::app::console_input::Mode::Shell, "shell mode should stick");
}

} // namespace

void checkConsoleInput() {
	checkInjectedQueuePrecedesPlatformInput();
	checkInjectedSequenceIsAtomic();
	checkReadAvailableDrainsPlatformInputBatch();
	checkReadAvailableHonorsOutputCapacity();
	checkModeState();
}
