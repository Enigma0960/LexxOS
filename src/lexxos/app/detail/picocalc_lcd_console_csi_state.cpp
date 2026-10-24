#include <lexxos/app/detail/picocalc_lcd_console_csi_state.h>

namespace lexxos::app::picocalc_lcd_console::detail {

void LcdConsoleCsiState::reset() {
	for (auto& parameter : _parameters) {
		parameter = 0;
	}
	_parameterCount = 0;
	_currentValue = 0;
	_currentPresent = false;
	_private = false;
	_ignore = false;
}

bool LcdConsoleCsiState::consume(char value) {
	if (value == '?' && _parameterCount == 0 && !_currentPresent) {
		_private = true;
		return true;
	}

	if (value >= '0' && value <= '9') {
		if (!_ignore && _currentValue < 1000) {
			_currentValue = _currentValue * 10 + value - '0';
			_currentPresent = true;
		}
		return true;
	}

	if (value == ';') {
		finishParameter();
		return true;
	}

	if (value >= 0x20 && value <= 0x2F) {
		_ignore = true;
		return true;
	}

	return false;
}

bool LcdConsoleCsiState::isFinal(char value) const {
	return value >= 0x40 && value <= 0x7E;
}

void LcdConsoleCsiState::finishParameter() {
	if (_ignore) {
		return;
	}
	if (_parameterCount >= _parameters.size()) {
		_ignore = true;
		return;
	}

	_parameters[_parameterCount++] = _currentPresent ? _currentValue : 0;
	_currentValue = 0;
	_currentPresent = false;
}

int LcdConsoleCsiState::parameter(std::size_t index, int defaultValue) const {
	if (index >= _parameterCount) {
		return defaultValue;
	}
	return _parameters[index] == 0 ? defaultValue : _parameters[index];
}

int LcdConsoleCsiState::parameterCount() const {
	return static_cast<int>(_parameterCount);
}

int LcdConsoleCsiState::countParameter() const {
	auto const count = parameter(0, 1);
	return count > 0 ? count : 1;
}

bool LcdConsoleCsiState::isPrivate() const {
	return _private;
}

} // namespace lexxos::app::picocalc_lcd_console::detail
