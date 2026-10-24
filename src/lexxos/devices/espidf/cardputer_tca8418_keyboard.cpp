#include <lexxos/devices/espidf/cardputer_tca8418_keyboard.h>

#include <algorithm>

#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/platform/platform.h>

namespace lexxos::devices {
namespace {

	constexpr std::uint8_t tca8418RegisterConfig = 0x01;
	constexpr std::uint8_t tca8418RegisterInterruptStatus = 0x02;
	constexpr std::uint8_t tca8418RegisterKeyLockEventCount = 0x03;
	constexpr std::uint8_t tca8418RegisterKeyEventA = 0x04;
	constexpr std::uint8_t tca8418RegisterGpioDirection1 = 0x23;
	constexpr std::uint8_t tca8418RegisterGpioDirection2 = 0x24;
	constexpr std::uint8_t tca8418RegisterGpioDirection3 = 0x25;
	constexpr std::uint8_t tca8418RegisterGpioPull1 = 0x2C;
	constexpr std::uint8_t tca8418RegisterGpioPull2 = 0x2D;
	constexpr std::uint8_t tca8418RegisterGpioPull3 = 0x2E;
	constexpr std::uint8_t tca8418RegisterKeypadGpio1 = 0x1D;
	constexpr std::uint8_t tca8418RegisterKeypadGpio2 = 0x1E;
	constexpr std::uint8_t tca8418RegisterKeypadGpio3 = 0x1F;
	constexpr std::uint8_t tca8418ConfigKeyEventInterruptEnable = 0x01;
	constexpr std::uint8_t tca8418ConfigOverflowInterruptEnable = 0x08;
	constexpr std::uint8_t tca8418ConfigOverflowMode = 0x20;
	constexpr std::uint8_t tca8418InterruptKeyEvent = 0x01;
	constexpr std::uint8_t tca8418InterruptOverflow = 0x08;
	constexpr std::uint8_t tca8418EventCountMask = 0x0F;
	constexpr std::uint8_t tca8418Rows = 7;
	constexpr std::uint8_t tca8418Columns = 8;
	constexpr std::uint32_t pressDebounceMs = 70;
	constexpr std::uint32_t releaseDebounceMs = 25;

	using Key = CardputerTca8418Keyboard::KeyDefinition;
	using Mod = CardputerTca8418Keyboard::Modifier;

	struct KeyPosition {
		std::uint8_t row{0};
		std::uint8_t column{0};
	};

	constexpr Key keyMap[CardputerTca8418Keyboard::rowCount][CardputerTca8418Keyboard::columnCount] = {
	    {
         {KeyCode::Grave, '`', '~', Mod::None, KeyCode::Escape},
         {KeyCode::Num1, '1', '!', Mod::None, KeyCode::F1},
         {KeyCode::Num2, '2', '@', Mod::None, KeyCode::F2},
         {KeyCode::Num3, '3', '#', Mod::None, KeyCode::F3},
         {KeyCode::Num4, '4', '$', Mod::None, KeyCode::F4},
         {KeyCode::Num5, '5', '%', Mod::None, KeyCode::F5},
         {KeyCode::Num6, '6', '^', Mod::None, KeyCode::F6},
         {KeyCode::Num7, '7', '&', Mod::None, KeyCode::F7},
         {KeyCode::Num8, '8', '*', Mod::None, KeyCode::F8},
         {KeyCode::Num9, '9', '(', Mod::None, KeyCode::F9},
         {KeyCode::Num0, '0', ')', Mod::None, KeyCode::F10},
         {KeyCode::Minus, '-', '_'},
         {KeyCode::Equals, '=', '+'},
         {KeyCode::Backspace, 0, 0, Mod::None, KeyCode::Delete},
	     },
	    {
         {KeyCode::Tab, '\t', '\t'},
         {KeyCode::Q, 'q', 'Q'},
         {KeyCode::W, 'w', 'W'},
         {KeyCode::E, 'e', 'E'},
         {KeyCode::R, 'r', 'R'},
         {KeyCode::T, 't', 'T'},
         {KeyCode::Y, 'y', 'Y'},
         {KeyCode::U, 'u', 'U'},
         {KeyCode::I, 'i', 'I'},
         {KeyCode::O, 'o', 'O'},
         {KeyCode::P, 'p', 'P'},
         {KeyCode::LeftBracket, '[', '{'},
         {KeyCode::RightBracket, ']', '}'},
         {KeyCode::Backslash, '\\', '|'},
	     },
	    {
         {KeyCode::Unknown, 0, 0, Mod::Fn},
         {KeyCode::Unknown, 0, 0, Mod::Shift},
         {KeyCode::A, 'a', 'A'},
         {KeyCode::S, 's', 'S'},
         {KeyCode::D, 'd', 'D'},
         {KeyCode::F, 'f', 'F'},
         {KeyCode::G, 'g', 'G'},
         {KeyCode::H, 'h', 'H'},
         {KeyCode::J, 'j', 'J'},
         {KeyCode::K, 'k', 'K'},
         {KeyCode::L, 'l', 'L'},
         {KeyCode::Semicolon, ';', ':', Mod::None, KeyCode::Up},
         {KeyCode::Apostrophe, '\'', '"'},
         {KeyCode::Enter, '\r', '\r'},
	     },
	    {
         {KeyCode::Unknown, 0, 0, Mod::Ctrl},
         {KeyCode::Unknown, 0, 0, Mod::Opt},
         {KeyCode::Unknown, 0, 0, Mod::Alt},
         {KeyCode::Z, 'z', 'Z'},
         {KeyCode::X, 'x', 'X'},
         {KeyCode::C, 'c', 'C'},
         {KeyCode::V, 'v', 'V'},
         {KeyCode::B, 'b', 'B'},
         {KeyCode::N, 'n', 'N'},
         {KeyCode::M, 'm', 'M'},
         {KeyCode::Comma, ',', '<', Mod::None, KeyCode::Left},
         {KeyCode::Period, '.', '>', Mod::None, KeyCode::Down},
         {KeyCode::Slash, '/', '?', Mod::None, KeyCode::Right},
         {KeyCode::Space, ' ', ' '},
	     },
	};

	KeyPosition remapTcaPosition(std::uint8_t row, std::uint8_t column) {
		return {
		    .row = static_cast<std::uint8_t>((column + 4U) % 4U),
		    .column = static_cast<std::uint8_t>(row * 2U + (column > 3U ? 1U : 0U)),
		};
	}

} // namespace

CardputerTca8418Keyboard::CardputerTca8418Keyboard(I2cBus& bus, std::uint8_t address)
    : _bus(bus)
    , _address(address) {
}

bool CardputerTca8418Keyboard::initialize() {
	_ready = false;
	_eventHead = 0;
	_eventTail = 0;
	_eventCount = 0;
	_physicalState = 0;
	_stableState = 0;
	_pendingMask = 0;
	_pendingTargetState = 0;
	_pendingSinceMs.fill(0);
	_stableModifiers = 0;
	_pressedKeys.fill(false);

	if (!_bus.isReady() || !_bus.probe(_address) || !_configureController()) {
		return false;
	}

	_ready = true;
	return true;
}

void CardputerTca8418Keyboard::update() {
	if (!_ready) {
		return;
	}

	auto const now = lexxos::platform::uptimeMs();
	std::uint8_t interruptStatus = 0;
	bool const interruptStatusValid = _readRegister(tca8418RegisterInterruptStatus, interruptStatus);
	if (interruptStatusValid && (interruptStatus & tca8418InterruptOverflow) != 0) {
		_drainControllerEvents();
		_resetStateAfterControllerOverflow();
		(void) _writeRegister(tca8418RegisterInterruptStatus, tca8418InterruptKeyEvent | tca8418InterruptOverflow);
		return;
	}

	for (std::uint8_t index = 0; index < eventQueueCapacity; ++index) {
		RawKeyEvent event{};
		if (!_readEvent(event)) {
			break;
		}
		if (event.valid) {
			_setPhysicalKeyState(event.row, event.column, event.pressed);
		}
	}
	_applyDebouncedState(now);

	std::uint8_t ignored{};
	if (_readRegister(tca8418RegisterInterruptStatus, ignored)) {
		(void) _writeRegister(tca8418RegisterInterruptStatus, tca8418InterruptKeyEvent | tca8418InterruptOverflow);
	}
}

std::string_view CardputerTca8418Keyboard::name() const {
	return "cardputer-keyboard";
}

bool CardputerTca8418Keyboard::isReady() const {
	return _ready;
}

bool CardputerTca8418Keyboard::pollEvent(InputEvent& event) {
	if (_eventCount == 0) {
		update();
	}
	return _popEvent(event);
}

bool CardputerTca8418Keyboard::isPressed(KeyCode key) const {
	auto const index = _keyIndex(key);
	return index < _pressedKeys.size() && _pressedKeys[index];
}

std::uint32_t CardputerTca8418Keyboard::rawState() const {
	return static_cast<std::uint32_t>(_stableState & 0xFFFFFFFFULL);
}

std::uint32_t CardputerTca8418Keyboard::rawKeyCount() const {
	return keyCount;
}

std::uint32_t CardputerTca8418Keyboard::rawRowCount() const {
	return rowCount;
}

std::uint32_t CardputerTca8418Keyboard::rawColumnCount() const {
	return columnCount;
}

bool CardputerTca8418Keyboard::_writeRegister(std::uint8_t reg, std::uint8_t value) {
	std::uint8_t data[] = {reg, value};
	return _bus.write(_address, data, sizeof(data));
}

bool CardputerTca8418Keyboard::_readRegister(std::uint8_t reg, std::uint8_t& value) {
	I2cBusTransaction transaction{_bus};
	return transaction && _bus.write(_address, &reg, 1) && _bus.read(_address, &value, 1);
}

bool CardputerTca8418Keyboard::_configureController() {
	if (!_writeRegister(tca8418RegisterGpioDirection1, 0x00)
	    || !_writeRegister(tca8418RegisterGpioDirection2, 0x00)
	    || !_writeRegister(tca8418RegisterGpioDirection3, 0x00)
	    || !_writeRegister(tca8418RegisterGpioPull1, 0x00)
	    || !_writeRegister(tca8418RegisterGpioPull2, 0x00)
	    || !_writeRegister(tca8418RegisterGpioPull3, 0x00)) {
		return false;
	}

	std::uint8_t rowMask = 0;
	for (std::uint8_t row = 0; row < tca8418Rows; ++row) {
		rowMask = static_cast<std::uint8_t>((rowMask << 1U) | 1U);
	}

	std::uint8_t columnMask = 0;
	for (std::uint8_t column = 0; column < tca8418Columns; ++column) {
		columnMask = static_cast<std::uint8_t>((columnMask << 1U) | 1U);
	}

	if (!_writeRegister(tca8418RegisterKeypadGpio1, rowMask)
	    || !_writeRegister(tca8418RegisterKeypadGpio2, columnMask)
	    || !_writeRegister(tca8418RegisterKeypadGpio3, 0x00)) {
		return false;
	}

	RawKeyEvent ignored{};
	while (_readEvent(ignored)) {
	}
	_resetStateAfterControllerOverflow();

	return _writeRegister(
	           tca8418RegisterConfig,
	           static_cast<std::uint8_t>(tca8418ConfigKeyEventInterruptEnable | tca8418ConfigOverflowInterruptEnable | tca8418ConfigOverflowMode))
	    && _writeRegister(tca8418RegisterInterruptStatus, tca8418InterruptKeyEvent | tca8418InterruptOverflow);
}

std::uint8_t CardputerTca8418Keyboard::_availableEvents() {
	std::uint8_t value = 0;
	if (!_readRegister(tca8418RegisterKeyLockEventCount, value)) {
		return 0;
	}
	return value & tca8418EventCountMask;
}

bool CardputerTca8418Keyboard::_readEvent(RawKeyEvent& event) {
	event = {};
	if (_availableEvents() == 0) {
		return false;
	}

	std::uint8_t raw = 0;
	if (!_readRegister(tca8418RegisterKeyEventA, raw)) {
		return false;
	}

	auto const rawIndex = raw & 0x7FU;
	if (rawIndex == 0) {
		return true;
	}

	event.pressed = (raw & 0x80U) != 0;
	auto const index = static_cast<std::uint8_t>(rawIndex - 1U);
	auto const tcaRow = static_cast<std::uint8_t>(index / 10U);
	auto const tcaColumn = static_cast<std::uint8_t>(index % 10U);
	if (tcaRow >= tca8418Rows || tcaColumn >= tca8418Columns) {
		return true;
	}

	auto const position = remapTcaPosition(tcaRow, tcaColumn);
	if (position.row >= rowCount || position.column >= columnCount) {
		return true;
	}

	event.valid = true;
	event.row = position.row;
	event.column = position.column;
	return true;
}

void CardputerTca8418Keyboard::_drainControllerEvents() {
	RawKeyEvent ignored{};
	for (std::uint8_t index = 0; index < eventQueueCapacity; ++index) {
		if (!_readEvent(ignored)) {
			break;
		}
	}
}

void CardputerTca8418Keyboard::_resetStateAfterControllerOverflow() {
	_eventHead = 0;
	_eventTail = 0;
	_eventCount = 0;
	_physicalState = 0;
	_stableState = 0;
	_pendingMask = 0;
	_pendingTargetState = 0;
	_pendingSinceMs.fill(0);
	_stableModifiers = 0;
	_pressedKeys.fill(false);
}

void CardputerTca8418Keyboard::_setPhysicalKeyState(std::uint8_t row, std::uint8_t column, bool pressed) {
	auto const bit = _keyBit(row, column);
	if (pressed) {
		_physicalState |= bit;
	} else {
		_physicalState &= ~bit;
	}
}

void CardputerTca8418Keyboard::_applyDebouncedState(std::uint32_t timestampMs) {
	for (std::uint8_t row = 0; row < rowCount; ++row) {
		for (std::uint8_t column = 0; column < columnCount; ++column) {
			auto const bit = _keyBit(row, column);
			auto const physicalPressed = (_physicalState & bit) != 0;
			auto const stablePressed = (_stableState & bit) != 0;
			if (physicalPressed == stablePressed) {
				_clearPendingKey(row, column);
				continue;
			}

			auto const pendingTargetPressed = (_pendingTargetState & bit) != 0;
			if ((_pendingMask & bit) == 0 || pendingTargetPressed != physicalPressed) {
				_pendingMask |= bit;
				if (physicalPressed) {
					_pendingTargetState |= bit;
				} else {
					_pendingTargetState &= ~bit;
				}
				_pendingSinceMs[_scanCode(row, column)] = timestampMs;
				continue;
			}

			auto const requiredMs = physicalPressed ? pressDebounceMs : releaseDebounceMs;
			if (timestampMs - _pendingSinceMs[_scanCode(row, column)] >= requiredMs) {
				_clearPendingKey(row, column);
				_setKeyState(row, column, physicalPressed, timestampMs);
			}
		}
	}
}

void CardputerTca8418Keyboard::_clearPendingKey(std::uint8_t row, std::uint8_t column) {
	auto const bit = _keyBit(row, column);
	_pendingMask &= ~bit;
	_pendingTargetState &= ~bit;
	_pendingSinceMs[_scanCode(row, column)] = 0;
}

void CardputerTca8418Keyboard::_setKeyState(std::uint8_t row, std::uint8_t column, bool pressed, std::uint32_t timestampMs) {
	auto const bit = _keyBit(row, column);
	auto const wasPressed = (_stableState & bit) != 0;
	if (wasPressed == pressed) {
		return;
	}

	if (pressed) {
		_stableState |= bit;
	} else {
		_stableState &= ~bit;
	}
	_stableModifiers = _modifiersForState(_stableState);
	_updatePressedKeyCache();

	auto const& key = _keyAt(row, column);
	if (key.modifier == Modifier::None) {
		_pushEvent(pressed ? InputEventType::KeyDown : InputEventType::KeyUp, row, column, timestampMs);
	}
}

void CardputerTca8418Keyboard::_updatePressedKeyCache() {
	_pressedKeys.fill(false);
	for (std::uint8_t row = 0; row < rowCount; ++row) {
		for (std::uint8_t column = 0; column < columnCount; ++column) {
			if (!_isPressed(_stableState, row, column)) {
				continue;
			}
			auto const index = _keyIndex(_keyAt(row, column).key);
			if (index < _pressedKeys.size()) {
				_pressedKeys[index] = true;
			}
		}
	}
}

void CardputerTca8418Keyboard::_pushEvent(InputEventType type, std::uint8_t row, std::uint8_t column, std::uint32_t timestampMs) {
	auto const& definition = _keyAt(row, column);
	if (definition.key == KeyCode::Unknown && definition.normal == 0) {
		return;
	}

	if (_eventCount >= _events.size()) {
		_eventTail = (_eventTail + 1) % _events.size();
		--_eventCount;
	}

	auto const fnPressed = _isPressed(_stableState, 2, 0);
	KeyCode eventKey = definition.key;
	std::uint8_t character = 0;
	auto eventModifiers = _stableModifiers;

	if (fnPressed) {
		if (definition.fnKey == KeyCode::Unknown) {
			return;
		}
		eventKey = definition.fnKey;
		// Fn is represented internally as InputModifierSym, but the translated
		// Fn-layer key must behave like a standalone key.  Otherwise Fn+arrows are
		// treated by the console as modified arrows and enter scrollback mode.
		eventModifiers &= static_cast<std::uint8_t>(~InputModifierSym);
	} else if (type == InputEventType::KeyDown) {
		auto const shifted = (_stableModifiers & (InputModifierShiftLeft | InputModifierShiftRight)) != 0;
		character = static_cast<std::uint8_t>(shifted && definition.shifted != 0 ? definition.shifted : definition.normal);
	}

	if (eventKey == KeyCode::Unknown && character == 0) {
		return;
	}

	_events[_eventHead] = {
	    .type = type,
	    .key = eventKey,
	    .timestampMs = timestampMs,
	    .character = character,
	    .scanCode = _scanCode(row, column),
	    .modifiers = eventModifiers,
	};
	_eventHead = (_eventHead + 1) % _events.size();
	++_eventCount;
}

bool CardputerTca8418Keyboard::_popEvent(InputEvent& event) {
	if (_eventCount == 0) {
		return false;
	}
	event = _events[_eventTail];
	_eventTail = (_eventTail + 1) % _events.size();
	--_eventCount;
	return true;
}

std::uint8_t CardputerTca8418Keyboard::_modifiersForState(std::uint64_t state) const {
	std::uint8_t modifiers = 0;
	if (_isPressed(state, 2, 0)) {
		modifiers |= InputModifierSym;
	}
	if (_isPressed(state, 2, 1)) {
		modifiers |= InputModifierShiftLeft;
	}
	if (_isPressed(state, 3, 0)) {
		modifiers |= InputModifierCtrl;
	}
	if (_isPressed(state, 3, 1)) {
		modifiers |= InputModifierSym;
	}
	if (_isPressed(state, 3, 2)) {
		modifiers |= InputModifierAlt;
	}
	return modifiers;
}

std::uint8_t CardputerTca8418Keyboard::_scanCode(std::uint8_t row, std::uint8_t column) {
	return static_cast<std::uint8_t>(row * columnCount + column);
}

std::uint64_t CardputerTca8418Keyboard::_keyBit(std::uint8_t row, std::uint8_t column) {
	return 1ULL << _scanCode(row, column);
}

bool CardputerTca8418Keyboard::_isPressed(std::uint64_t state, std::uint8_t row, std::uint8_t column) {
	return (state & _keyBit(row, column)) != 0;
}

CardputerTca8418Keyboard::KeyDefinition const& CardputerTca8418Keyboard::_keyAt(std::uint8_t row, std::uint8_t column) {
	return keyMap[row][column];
}

std::uint32_t CardputerTca8418Keyboard::_keyIndex(KeyCode key) {
	return static_cast<std::uint32_t>(key);
}

} // namespace lexxos::devices
