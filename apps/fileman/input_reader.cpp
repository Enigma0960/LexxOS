#include "input_reader.h"

#include <lexx/lexx.h>

namespace fileman {
namespace {
	// ESC by itself is ambiguous with the beginning of an ANSI sequence.
	// Allow queued ESC [ X / ESC O X bytes to arrive before treating it as Escape.
	constexpr unsigned escapeIdlePolls = 4096;
} // namespace

InputKey InputReader::poll() {
	auto const value = lexx::read_char();
	if (value == lexx::no_data || value == lexx::cancelled || value > 0xffU) {
		if (_state != State::Plain && ++_idlePolls >= escapeIdlePolls) {
			bool const escapeAlone = _state == State::Escape;
			_state = State::Plain;
			_parameter = 0;
			_idlePolls = 0;
			return escapeAlone ? InputKey::Escape : InputKey::None;
		}
		return InputKey::None;
	}

	_idlePolls = 0;
	return consume(static_cast<unsigned char>(value));
}

InputKey InputReader::consume(unsigned char character) {
	if (_state == State::Escape) {
		if (character == '[') {
			_state = State::Csi;
			_parameter = 0;
			return InputKey::None;
		}
		if (character == 'O') {
			_state = State::Ss3;
			return InputKey::None;
		}
		_state = State::Plain;
		_parameter = 0;
		// An unknown Alt+letter combination isn't a normal command.
		return InputKey::None;
	}
	if (_state == State::Ss3) {
		_state = State::Plain;
		if (character == 'R') {
			return InputKey::View; // xterm F3
		}
		return InputKey::None;
	}
	if (_state == State::Csi) {
		if (character >= '0' && character <= '9') {
			if (_parameter < 100U) {
				_parameter = _parameter * 10U + character - '0';
			}
			return InputKey::None;
		}
		if (character == ';') {
			return InputKey::None; // optional modifier argument
		}
		auto const parameter = _parameter;
		_state = State::Plain;
		_parameter = 0;
		switch (character) {
			case 'A': return InputKey::Up;
			case 'B': return InputKey::Down;
			case 'C': return InputKey::Right;
			case 'D': return InputKey::Left;
			case 'Z': return InputKey::Tab; // Shift+Tab
			case 'H': return InputKey::PageUp; // Home: previous page
			case 'F': return InputKey::PageDown; // End: next page
			case '~':
				if (parameter == 3 || parameter == 19) {
					return InputKey::Delete; // Delete / F8
				}
				if (parameter == 15) {
					return InputKey::Copy; // F5
				}
				if (parameter == 17) {
					return InputKey::Move; // F6
				}
				if (parameter == 13) {
					return InputKey::View; // F3
				}
				if (parameter == 20) {
					return InputKey::Options; // F9
				}
				if (parameter == 5) {
					return InputKey::PageUp;
				}
				if (parameter == 6) {
					return InputKey::PageDown;
				}
				if (parameter == 21) {
					return InputKey::Quit; // F10
				}
				break;
			default: break;
		}
		return InputKey::None;
	}
	if (character == 0x1bU) {
		_state = State::Escape;
		return InputKey::None;
	}
	switch (character) {
		case '\r':
		case '\n': return InputKey::Enter;
		case '\b':
		case 0x7f: return InputKey::Backspace;
		case '\t': return InputKey::Tab;
		case 'k' :
		case 'K' : return InputKey::Up;
		case 'j' :
		case 'J' : return InputKey::Down;
		case 'h' :
		case 'H' : return InputKey::Left;
		case 'l' :
		case 'L' : return InputKey::Right;
		case 'v' :
		case 'V' : return InputKey::View;
		case 'c' :
		case 'C' : return InputKey::Copy;
		case 'm' :
		case 'M' : return InputKey::Move;
		case 'n' :
		case 'N' : return InputKey::Rename;
		case 'r' :
		case 'R' : return InputKey::Refresh;
		case 'o' :
		case 'O' : return InputKey::Options;
		case 'u' :
		case 'U' : return InputKey::PageUp;
		case 'd' :
		case 'D' : return InputKey::PageDown;
		case 'q' :
		case 'Q' : return InputKey::Quit;
		default  : return InputKey::None;
	}
}

} // namespace fileman
