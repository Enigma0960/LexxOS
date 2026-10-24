#include <lexxos/devices/input/matrix_keyboard.h>

#include <lexxos/platform/platform_services.h>

namespace lexxos::devices {

MatrixKeyboard::MatrixKeyboard(MatrixKeyboardConfig config)
    : MatrixKeyboard(config, lexxos::platform::services()) {
}

MatrixKeyboard::MatrixKeyboard(MatrixKeyboardConfig config, lexxos::platform::PlatformServices const& platform)
    : _platformServices(platform)
    , _config(config) {
}

bool MatrixKeyboard::initialize() {
	if (!_isConfigurationValid()) {
		return false;
	}

	auto const inactiveLevel = !_activeLevelToValue(_config.rowActiveLevel);
	auto const& gpio = _platformServices.gpio;
	if (gpio.configureOutput == nullptr || gpio.configureInput == nullptr) {
		return false;
	}
	for (std::size_t index = 0; index < _config.rowCount; ++index) {
		if (!gpio.configureOutput(_config.rows[index], inactiveLevel)) {
			return false;
		}
	}

	auto const pull = _config.columnActiveLevel == lexxos::platform::GpioActiveLevel::Low
	    ? lexxos::platform::GpioPull::Up
	    : lexxos::platform::GpioPull::Down;
	for (std::size_t index = 0; index < _config.columnCount; ++index) {
		if (!gpio.configureInput(_config.columns[index], pull)) {
			return false;
		}
	}

	_driveAllRowsInactive();
	_ready = true;
	_firstScan = true;
	_eventHead = 0;
	_eventTail = 0;
	_eventCount = 0;
	_pressedKeys = 0;
	return true;
}

void MatrixKeyboard::update() {
	if (!_ready) {
		return;
	}

	auto const now = _uptimeMs();
	if (!_firstScan && _config.scanPeriodMs > 0 && now - _lastScanMs < _config.scanPeriodMs) {
		return;
	}
	_lastScanMs = now;

	auto const rawState = _readRawState();
	if (_firstScan) {
		_firstScan = false;
		_lastRawChangeMs = now;
		_lastRawState = rawState;
		_applyStableState(rawState);
		return;
	}

	if (rawState != _lastRawState) {
		_lastRawState = rawState;
		_lastRawChangeMs = now;
		return;
	}

	if (rawState != _stableState && now - _lastRawChangeMs >= _config.debounceMs) {
		_applyStableState(rawState);
	}
}

std::string_view MatrixKeyboard::name() const {
	return _config.name;
}

bool MatrixKeyboard::isReady() const {
	return _ready;
}

bool MatrixKeyboard::pollEvent(InputEvent& event) {
	update();
	return _popEvent(event);
}

bool MatrixKeyboard::isPressed(KeyCode key) const {
	auto const mask = _keyMask(key);
	return mask != 0 && (_pressedKeys & mask) != 0;
}

std::uint32_t MatrixKeyboard::rawState() const {
	return _stableState;
}

std::uint32_t MatrixKeyboard::rawKeyCount() const {
	return static_cast<std::uint32_t>(_keyCount());
}

std::uint32_t MatrixKeyboard::rawRowCount() const {
	return static_cast<std::uint32_t>(_config.rowCount);
}

std::uint32_t MatrixKeyboard::rawColumnCount() const {
	return static_cast<std::uint32_t>(_config.columnCount);
}

bool MatrixKeyboard::_isConfigurationValid() const {
	if (_config.name.empty() || _config.rows == nullptr || _config.columns == nullptr || _config.keyMap == nullptr) {
		return false;
	}

	if (_config.rowCount == 0 || _config.columnCount == 0 || _keyCount() > maxKeys) {
		return false;
	}

	if (_config.keyMapCount < _keyCount()) {
		return false;
	}

	for (std::size_t index = 0; index < _config.rowCount; ++index) {
		if (!_config.rows[index].isValid()) {
			return false;
		}
	}

	for (std::size_t index = 0; index < _config.columnCount; ++index) {
		if (!_config.columns[index].isValid()) {
			return false;
		}
	}

	return true;
}

std::size_t MatrixKeyboard::_keyCount() const {
	return _config.rowCount * _config.columnCount;
}

std::uint32_t MatrixKeyboard::_readRawState() {
	std::uint32_t state = 0;

	for (std::size_t row = 0; row < _config.rowCount; ++row) {
		_driveAllRowsInactive();
		_driveRowActive(row);

		for (std::size_t column = 0; column < _config.columnCount; ++column) {
			if (_readColumnActive(column)) {
				auto const keyIndex = row * _config.columnCount + column;
				state |= (1U << keyIndex);
			}
		}
	}

	_driveAllRowsInactive();
	return state;
}

void MatrixKeyboard::_driveAllRowsInactive() {
	auto const inactiveLevel = !_activeLevelToValue(_config.rowActiveLevel);
	auto const& gpio = _platformServices.gpio;
	if (gpio.write == nullptr) {
		return;
	}
	for (std::size_t index = 0; index < _config.rowCount; ++index) {
		gpio.write(_config.rows[index], inactiveLevel);
	}
}

void MatrixKeyboard::_driveRowActive(std::size_t row) {
	if (row >= _config.rowCount) {
		return;
	}

	auto const& gpio = _platformServices.gpio;
	if (gpio.write != nullptr) {
		gpio.write(_config.rows[row], _activeLevelToValue(_config.rowActiveLevel));
	}
}

bool MatrixKeyboard::_readColumnActive(std::size_t column) const {
	if (column >= _config.columnCount) {
		return false;
	}

	bool value = false;
	auto const& gpio = _platformServices.gpio;
	if (gpio.read == nullptr || !gpio.read(_config.columns[column], value)) {
		return false;
	}

	return value == _activeLevelToValue(_config.columnActiveLevel);
}

void MatrixKeyboard::_applyStableState(std::uint32_t state) {
	auto const changed = _stableState ^ state;
	if (changed == 0) {
		return;
	}

	auto const now = _uptimeMs();
	for (std::size_t index = 0; index < _keyCount(); ++index) {
		auto const bit = 1U << index;
		if ((changed & bit) == 0) {
			continue;
		}

		auto const key = _config.keyMap[index];
		if (key == KeyCode::Unknown) {
			continue;
		}

		if ((state & bit) != 0) {
			_pressedKeys |= _keyMask(key);
			_pushEvent(InputEventType::KeyDown, key, now);
		} else {
			_pressedKeys &= ~_keyMask(key);
			_pushEvent(InputEventType::KeyUp, key, now);
		}
	}

	_stableState = state;
}

void MatrixKeyboard::_pushEvent(InputEventType type, KeyCode key, std::uint32_t timestampMs) {
	if (_eventCount >= _events.size()) {
		_eventTail = (_eventTail + 1) % _events.size();
		--_eventCount;
	}

	_events[_eventHead] = {
	    .type = type,
	    .key = key,
	    .timestampMs = timestampMs,
	};
	_eventHead = (_eventHead + 1) % _events.size();
	++_eventCount;
}

bool MatrixKeyboard::_popEvent(InputEvent& event) {
	if (_eventCount == 0) {
		return false;
	}

	event = _events[_eventTail];
	_eventTail = (_eventTail + 1) % _events.size();
	--_eventCount;
	return true;
}

std::uint32_t MatrixKeyboard::_uptimeMs() const {
	auto const& clock = _platformServices.clock;
	return clock.uptimeMs != nullptr ? clock.uptimeMs() : 0U;
}

bool MatrixKeyboard::_activeLevelToValue(lexxos::platform::GpioActiveLevel level) {
	return level == lexxos::platform::GpioActiveLevel::High;
}

std::uint32_t MatrixKeyboard::_keyMask(KeyCode key) {
	auto const index = static_cast<std::uint32_t>(key);
	if (index >= 32) {
		return 0;
	}
	return 1U << index;
}

} // namespace lexxos::devices
