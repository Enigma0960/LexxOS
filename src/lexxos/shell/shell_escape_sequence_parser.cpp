#include <lexxos/shell/shell_escape_sequence_parser.h>

namespace lexxos::shell::detail {
namespace {

	[[nodiscard]] ShellEscapeSequenceResult keyResult(ShellInputKey key) {
		return {.hasKey = true, .key = key};
	}

	[[nodiscard]] ShellEscapeSequenceResult noKey() {
		return {};
	}

} // namespace

void ShellEscapeSequenceParser::start() {
	_state = State::Escape;
	_parameter = 0;
	_parameterPresent = false;
	_ignoreParameters = false;
}

void ShellEscapeSequenceParser::reset() {
	_state = State::None;
	_parameter = 0;
	_parameterPresent = false;
	_ignoreParameters = false;
}

bool ShellEscapeSequenceParser::active() const {
	return _state != State::None;
}

ShellEscapeSequenceResult ShellEscapeSequenceParser::consume(char value) {
	switch (_state) {
		case State::Escape:
			return _consumeEscape(value);
		case State::Ss3:
			return _consumeSs3(value);
		case State::Csi:
			return _consumeCsi(value);
		case State::None:
			reset();
			return noKey();
	}
	reset();
	return noKey();
}

void ShellEscapeSequenceParser::_enterCsi() {
	_state = State::Csi;
	_parameter = 0;
	_parameterPresent = false;
	_ignoreParameters = false;
}

ShellEscapeSequenceResult ShellEscapeSequenceParser::_consumeEscape(char value) {
	if (value == '[') {
		_enterCsi();
		return noKey();
	}
	if (value == 'O') {
		_state = State::Ss3;
		return noKey();
	}
	if (value == '\x1B') {
		start();
		return noKey();
	}
	reset();
	return noKey();
}

ShellEscapeSequenceResult ShellEscapeSequenceParser::_consumeSs3(char value) {
	ShellEscapeSequenceResult result{};
	switch (value) {
		case 'A':
			result = keyResult(ShellInputKey::ArrowUp);
			break;
		case 'B':
			result = keyResult(ShellInputKey::ArrowDown);
			break;
		case 'C':
			result = keyResult(ShellInputKey::ArrowRight);
			break;
		case 'D':
			result = keyResult(ShellInputKey::ArrowLeft);
			break;
		case 'H':
			result = keyResult(ShellInputKey::Home);
			break;
		case 'F':
			result = keyResult(ShellInputKey::End);
			break;
		default:
			break;
	}
	reset();
	return result;
}

ShellEscapeSequenceResult ShellEscapeSequenceParser::_consumeCsi(char value) {
	if (value >= '0' && value <= '9') {
		if (!_ignoreParameters && _parameter < 1000U) {
			_parameter = static_cast<std::uint16_t>(_parameter * 10U + static_cast<unsigned>(value - '0'));
			_parameterPresent = true;
		}
		return noKey();
	}
	if (value == ';') {
		_ignoreParameters = true;
		return noKey();
	}

	ShellEscapeSequenceResult result{};
	switch (value) {
		case 'A':
			result = keyResult(ShellInputKey::ArrowUp);
			break;
		case 'B':
			result = keyResult(ShellInputKey::ArrowDown);
			break;
		case 'C':
			result = keyResult(ShellInputKey::ArrowRight);
			break;
		case 'D':
			result = keyResult(ShellInputKey::ArrowLeft);
			break;
		case 'H':
			result = keyResult(ShellInputKey::Home);
			break;
		case 'F':
			result = keyResult(ShellInputKey::End);
			break;
		case '~':
			if (_parameterPresent && _parameter == 3U) {
				result = keyResult(ShellInputKey::Delete);
			} else if (_parameterPresent && (_parameter == 1U || _parameter == 7U)) {
				result = keyResult(ShellInputKey::Home);
			} else if (_parameterPresent && (_parameter == 4U || _parameter == 8U)) {
				result = keyResult(ShellInputKey::End);
			}
			break;
		default:
			break;
	}
	reset();
	return result;
}

} // namespace lexxos::shell::detail
