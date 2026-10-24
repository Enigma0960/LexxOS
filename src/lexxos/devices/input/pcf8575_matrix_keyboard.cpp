#include <lexxos/devices/input/pcf8575_matrix_keyboard.h>

#include <lexxos/platform/platform_services.h>

namespace lexxos::devices {

Pcf8575MatrixKeyboard::Pcf8575MatrixKeyboard(Pcf8575PortExpander& expander, Pcf8575MatrixKeyboardConfig config)
    : Pcf8575MatrixKeyboard(expander, config, lexxos::platform::services()) {
}

Pcf8575MatrixKeyboard::Pcf8575MatrixKeyboard(
    Pcf8575PortExpander& expander,
    Pcf8575MatrixKeyboardConfig config,
    lexxos::platform::PlatformServices const& platform)
    : _expander(expander)
    , _platformServices(platform)
    , _config(config) {
}

bool Pcf8575MatrixKeyboard::initialize() {
	if (!_isConfigurationValid() || !_expander.isReady()) {
		_ready = false;
		return false;
	}

	_ready = _writeIdleState();
	_firstScan = true;
	_eventHead = 0;
	_eventTail = 0;
	_eventCount = 0;
	_pressedKeys = 0;
	return _ready;
}

void Pcf8575MatrixKeyboard::update() {
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

std::string_view Pcf8575MatrixKeyboard::name() const {
	return _config.name;
}

bool Pcf8575MatrixKeyboard::isReady() const {
	return _ready && _expander.isReady();
}

bool Pcf8575MatrixKeyboard::pollEvent(InputEvent& event) {
	update();
	return _popEvent(event);
}

bool Pcf8575MatrixKeyboard::isPressed(KeyCode key) const {
	auto const mask = _keyMask(key);
	return mask != 0 && (_pressedKeys & mask) != 0;
}

std::uint32_t Pcf8575MatrixKeyboard::rawState() const {
	return _stableState;
}

std::uint32_t Pcf8575MatrixKeyboard::rawKeyCount() const {
	return static_cast<std::uint32_t>(_keyCount());
}

std::uint32_t Pcf8575MatrixKeyboard::rawRowCount() const {
	return static_cast<std::uint32_t>(_config.rowCount);
}

std::uint32_t Pcf8575MatrixKeyboard::rawColumnCount() const {
	return static_cast<std::uint32_t>(_config.columnCount);
}

bool Pcf8575MatrixKeyboard::_isConfigurationValid() const {
	if (_config.name.empty() || _config.rows == nullptr || _config.columns == nullptr || _config.keyMap == nullptr) {
		return false;
	}

	if (_config.rowCount == 0 || _config.columnCount == 0 || _keyCount() > maxKeys) {
		return false;
	}

	if (_config.keyMapCount < _keyCount()) {
		return false;
	}

	if (_config.rowActiveLevel != lexxos::platform::GpioActiveLevel::Low || _config.columnActiveLevel != lexxos::platform::GpioActiveLevel::Low) {
		return false;
	}

	for (std::size_t index = 0; index < _config.rowCount; ++index) {
		if (_config.rows[index] >= 16) {
			return false;
		}
	}

	for (std::size_t index = 0; index < _config.columnCount; ++index) {
		if (_config.columns[index] >= 16) {
			return false;
		}
	}

	return true;
}

std::size_t Pcf8575MatrixKeyboard::_keyCount() const {
	return _config.rowCount * _config.columnCount;
}

std::uint16_t Pcf8575MatrixKeyboard::_pinMask(std::uint8_t pin) const {
	return static_cast<std::uint16_t>(1U << pin);
}

std::uint32_t Pcf8575MatrixKeyboard::_readRawState() {
	auto transaction = _expander.transaction();
	if (!transaction) {
		return _stableState;
	}

	std::uint32_t state = 0;

	for (std::size_t row = 0; row < _config.rowCount; ++row) {
		if (!_driveAllRowsInactive() || !_driveRowActive(row)) {
			_writeIdleState();
			return _stableState;
		}

		std::uint16_t port = 0xFFFF;
		if (!_expander.readPort(port)) {
			_writeIdleState();
			return _stableState;
		}

		for (std::size_t column = 0; column < _config.columnCount; ++column) {
			if (_readColumnActive(port, column)) {
				auto const keyIndex = row * _config.columnCount + column;
				state |= (1U << keyIndex);
			}
		}
	}

	_writeIdleState();
	return state & ~_config.ignoredKeyMask;
}

bool Pcf8575MatrixKeyboard::_writeIdleState() {
	std::uint16_t value = 0xFFFF;
	for (std::size_t row = 0; row < _config.rowCount; ++row) {
		value &= static_cast<std::uint16_t>(~_pinMask(_config.rows[row]));
	}
	return _expander.writePort(value);
}

bool Pcf8575MatrixKeyboard::_driveAllRowsInactive() {
	return _expander.writePort(0xFFFF);
}

bool Pcf8575MatrixKeyboard::_driveRowActive(std::size_t row) {
	if (row >= _config.rowCount) {
		return false;
	}

	auto value = static_cast<std::uint16_t>(0xFFFF & ~_pinMask(_config.rows[row]));
	return _expander.writePort(value);
}

bool Pcf8575MatrixKeyboard::_readColumnActive(std::uint16_t port, std::size_t column) const {
	if (column >= _config.columnCount) {
		return false;
	}

	auto const mask = _pinMask(_config.columns[column]);
	return (port & mask) == 0;
}

void Pcf8575MatrixKeyboard::_applyStableState(std::uint32_t state) {
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

void Pcf8575MatrixKeyboard::_pushEvent(InputEventType type, KeyCode key, std::uint32_t timestampMs) {
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

bool Pcf8575MatrixKeyboard::_popEvent(InputEvent& event) {
	if (_eventCount == 0) {
		return false;
	}

	event = _events[_eventTail];
	_eventTail = (_eventTail + 1) % _events.size();
	--_eventCount;
	return true;
}

std::uint32_t Pcf8575MatrixKeyboard::_uptimeMs() const {
	auto const& clock = _platformServices.clock;
	return clock.uptimeMs != nullptr ? clock.uptimeMs() : 0U;
}

std::uint32_t Pcf8575MatrixKeyboard::_keyMask(KeyCode key) {
	auto const index = static_cast<std::uint32_t>(key);
	if (index >= 32) {
		return 0;
	}
	return 1U << index;
}

} // namespace lexxos::devices
