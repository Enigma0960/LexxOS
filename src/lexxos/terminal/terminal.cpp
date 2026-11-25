#include <lexxos/terminal/terminal.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <lexxos/app/console_input_service.h>
#include <lexxos/io/output_stream.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/kernel/system.h>
#include <lexxos/os/app_context.h>
#include <lexxos/platform/platform_services.h>

namespace lexxos::terminal {

namespace {

	bool readConsoleCharBlocking(lexxos::platform::PlatformServices const& platform, lexxos::app::ConsoleInputService& input, char& value) {
		if (platform.console.readChar == nullptr && input.pendingInjected() == 0) {
			return false;
		}

		while (!lexxos::kernel::isShutdownRequested()) {
			if (input.readChar(platform, value)) {
				return true;
			}

			lexxos::kernel::sleepMs(1);
		}

		return false;
	}

	bool readConsoleCharWithTimeout(lexxos::platform::PlatformServices const& platform, lexxos::app::ConsoleInputService& input, char& value, std::uint32_t timeoutMs) {
		if (platform.clock.uptimeMs == nullptr) {
			return false;
		}

		auto const start = platform.clock.uptimeMs();
		while (!lexxos::kernel::isShutdownRequested()) {
			if (input.readChar(platform, value)) {
				return true;
			}

			if ((platform.clock.uptimeMs() - start) >= timeoutMs) {
				return false;
			}

			lexxos::kernel::sleepMs(1);
		}

		return false;
	}

	Key readCsiSequence(lexxos::platform::PlatformServices const& platform, lexxos::app::ConsoleInputService& input) {
		char value{};
		if (!readConsoleCharWithTimeout(platform, input, value, 20)) {
			return {.code = KeyCode::Escape};
		}

		if (value >= '0' && value <= '9') {
			std::uint16_t parameter = static_cast<std::uint16_t>(value - '0');
			while (readConsoleCharWithTimeout(platform, input, value, 20)) {
				if (value >= '0' && value <= '9') {
					parameter = static_cast<std::uint16_t>((parameter * 10U) + static_cast<std::uint16_t>(value - '0'));
					continue;
				}

				if (value == ';') {
					continue;
				}

				if (value != '~') {
					return {.code = KeyCode::Unknown};
				}

				switch (parameter) {
					case 1:
					case 7:
						return {.code = KeyCode::Home};
					case 3:
						return {.code = KeyCode::Delete};
					case 4:
					case 8:
						return {.code = KeyCode::End};
					case 5:
						return {.code = KeyCode::PageUp};
					case 6:
						return {.code = KeyCode::PageDown};
					default:
						return {.code = KeyCode::Unknown};
				}
			}

			return {.code = KeyCode::Unknown};
		}

		switch (value) {
			case 'A':
				return {.code = KeyCode::ArrowUp};
			case 'B':
				return {.code = KeyCode::ArrowDown};
			case 'C':
				return {.code = KeyCode::ArrowRight};
			case 'D':
				return {.code = KeyCode::ArrowLeft};
			case 'H':
				return {.code = KeyCode::Home};
			case 'F':
				return {.code = KeyCode::End};
			default:
				return {.code = KeyCode::Unknown};
		}
	}

	Key readSs3Sequence(lexxos::platform::PlatformServices const& platform, lexxos::app::ConsoleInputService& input) {
		char value{};
		if (!readConsoleCharWithTimeout(platform, input, value, 20)) {
			return {.code = KeyCode::Escape};
		}

		switch (value) {
			case 'A':
				return {.code = KeyCode::ArrowUp};
			case 'B':
				return {.code = KeyCode::ArrowDown};
			case 'C':
				return {.code = KeyCode::ArrowRight};
			case 'D':
				return {.code = KeyCode::ArrowLeft};
			case 'H':
				return {.code = KeyCode::Home};
			case 'F':
				return {.code = KeyCode::End};
			default:
				return {.code = KeyCode::Unknown};
		}
	}

	void appendUnsigned(std::array<char, 16>& buffer, std::size_t& length, std::uint8_t value) {
		char digits[4]{};
		std::size_t digitCount = 0;
		if (value == 0) {
			digits[digitCount++] = '0';
		} else {
			while (value != 0 && digitCount < sizeof(digits)) {
				digits[digitCount++] = static_cast<char>('0' + (value % 10U));
				value = static_cast<std::uint8_t>(value / 10U);
			}
		}

		while (digitCount != 0 && length < buffer.size()) {
			buffer[length++] = digits[--digitCount];
		}
	}

} // namespace

bool isPrintableAscii(Key const& key) {
	auto const value = static_cast<unsigned char>(key.character);
	return key.code == KeyCode::Character && value >= 0x20U && value <= 0x7EU;
}

Key readKey(lexxos::platform::PlatformServices const& platform, lexxos::app::ConsoleInputService& input) {
	char value{};
	if (!readConsoleCharBlocking(platform, input, value)) {
		return {.code = KeyCode::Unknown};
	}

	switch (value) {
		case '\x03':
			return {.code = KeyCode::CtrlC};
		case '\x11':
			return {.code = KeyCode::CtrlQ};
		case '\x13':
			return {.code = KeyCode::CtrlS};
		case '\x18':
			return {.code = KeyCode::CtrlX};
		case '\r':
		case '\n':
			return {.code = KeyCode::Enter};
		case '\b':
		case 0x7F:
			return {.code = KeyCode::Backspace};
		case '\x1B': {
			char next{};
			if (!readConsoleCharWithTimeout(platform, input, next, 20)) {
				return {.code = KeyCode::Escape};
			}

			if (next == '[') {
				return readCsiSequence(platform, input);
			}

			if (next == 'O') {
				return readSs3Sequence(platform, input);
			}

			return {.code = KeyCode::Escape};
		}
		default:
			return {.code = KeyCode::Character, .character = value};
	}
}

void clearScreen(lexxos::io::OutputStream& output) {
	output.write("\x1B[2J\x1B[H");
}

void eraseLine(lexxos::io::OutputStream& output) {
	output.write("\x1B[2K");
}

void moveCursor(lexxos::io::OutputStream& output, std::uint8_t row, std::uint8_t column) {
	std::array<char, 16> sequence{};
	std::size_t length = 0;
	sequence[length++] = '\x1B';
	sequence[length++] = '[';
	appendUnsigned(sequence, length, row == 0 ? 1 : row);
	sequence[length++] = ';';
	appendUnsigned(sequence, length, column == 0 ? 1 : column);
	sequence[length++] = 'H';
	output.write({sequence.data(), length});
}

void hideCursor(lexxos::io::OutputStream& output) {
	output.write("\x1B[?25l");
}

void showCursor(lexxos::io::OutputStream& output) {
	output.write("\x1B[?25h");
}

Key readKey() {
	return readKey(lexxos::platform::services(), lexxos::app::console_input::defaultService());
}

Key readKey(lexxos::os::AppContext& context) {
	return readKey(context.platform(), context.consoleInput());
}

} // namespace lexxos::terminal
