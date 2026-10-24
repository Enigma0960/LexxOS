#include <lexxos/devices/espidf/cardputer_keyboard.h>

#include <esp_rom_sys.h>

#include <lexxos/platform/platform.h>
#include <lexxos/platform/platform_services.h>

namespace lexxos::devices {
namespace {

	constexpr lexxos::platform::GpioPin selectorPins[] = {{8}, {9}, {11}};

	constexpr CardputerKeyboard::InputPinMapping inputPins[] = {
	    {.pin = {13},  .leftHalfColumn = 0,  .rightHalfColumn = 1},
	    {.pin = {15},  .leftHalfColumn = 2,  .rightHalfColumn = 3},
	    { .pin = {3},  .leftHalfColumn = 4,  .rightHalfColumn = 5},
	    { .pin = {4},  .leftHalfColumn = 6,  .rightHalfColumn = 7},
	    { .pin = {5},  .leftHalfColumn = 8,  .rightHalfColumn = 9},
	    { .pin = {6}, .leftHalfColumn = 10, .rightHalfColumn = 11},
	    { .pin = {7}, .leftHalfColumn = 12, .rightHalfColumn = 13},
	};

	constexpr std::uint32_t debounceMs = 20;
	constexpr std::uint32_t selectorSettleDelayUs = 100;
	constexpr std::uint8_t selectorIdleValue = 0x07;

	using Key = CardputerKeyboard::KeyDefinition;
	using Mod = CardputerKeyboard::Modifier;

	struct KeyPosition {
		std::uint8_t row{0};
		std::uint8_t column{0};
	};

	constexpr Key keyMap[CardputerKeyboard::rowCount][CardputerKeyboard::columnCount] = {
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

	KeyPosition keyPositionForScan(std::uint8_t selector, CardputerKeyboard::InputPinMapping const& mapping) {
		return selector > 3
		    ? KeyPosition{.row = static_cast<std::uint8_t>(7 - selector), .column = mapping.leftHalfColumn}
		    : KeyPosition{.row = static_cast<std::uint8_t>(3 - selector), .column = mapping.rightHalfColumn};
	}

} // namespace

CardputerKeyboard::CardputerKeyboard(lexxos::platform::PlatformServices const& platform)
    : _platformServices(platform) {
}

bool CardputerKeyboard::initialize() {
	_ready = false;
	_eventHead = 0;
	_eventTail = 0;
	_eventCount = 0;
	_pressedKeys.fill(false);

	if (!_configurePins()) {
		return false;
	}

	_ready = true;
	_lastRawState = _readRawState();
	_stableState = _lastRawState;
	_stableModifiers = _modifiersForState(_stableState);
	_updatePressedKeyCache(_stableState);
	_lastRawChangeMs = _uptimeMs();
	return true;
}

void CardputerKeyboard::update() {
	if (!_ready) {
		return;
	}

	auto const now = _uptimeMs();
	auto const rawState = _readRawState();
	if (rawState != _lastRawState) {
		_lastRawState = rawState;
		_lastRawChangeMs = now;
		return;
	}

	if (rawState != _stableState && now - _lastRawChangeMs >= debounceMs) {
		_applyStableState(rawState);
	}
}

std::string_view CardputerKeyboard::name() const {
	return "cardputer-keyboard";
}

bool CardputerKeyboard::isReady() const {
	return _ready;
}

bool CardputerKeyboard::pollEvent(InputEvent& event) {
	if (_eventCount == 0) {
		update();
	}
	return _popEvent(event);
}

bool CardputerKeyboard::isPressed(KeyCode key) const {
	auto const index = _keyIndex(key);
	return index < _pressedKeys.size() && _pressedKeys[index];
}

std::uint32_t CardputerKeyboard::rawState() const {
	return static_cast<std::uint32_t>(_stableState & 0xFFFFFFFFULL);
}

std::uint32_t CardputerKeyboard::rawKeyCount() const {
	return keyCount;
}

std::uint32_t CardputerKeyboard::rawRowCount() const {
	return rowCount;
}

std::uint32_t CardputerKeyboard::rawColumnCount() const {
	return columnCount;
}

bool CardputerKeyboard::_configurePins() {
	auto const& gpio = _platformServices.gpio;
	if (gpio.configureOutput == nullptr || gpio.configureInput == nullptr || gpio.write == nullptr || gpio.read == nullptr) {
		return false;
	}

	for (auto const pin : selectorPins) {
		// Keep the 74HC138 selector in an inactive/last-row state until a scan starts.
		if (!gpio.configureOutput(pin, true)) {
			return false;
		}
	}

	for (auto const& mapping : inputPins) {
		if (!gpio.configureInput(mapping.pin, lexxos::platform::GpioPull::Up)) {
			return false;
		}
	}

	_selectOutput(selectorIdleValue);
	esp_rom_delay_us(selectorSettleDelayUs);
	return true;
}

std::uint64_t CardputerKeyboard::_readRawState() const {
	std::uint64_t state = 0;
	auto const& gpio = _platformServices.gpio;
	if (gpio.read == nullptr) {
		return state;
	}

	for (std::uint8_t selector = 0; selector < 8; ++selector) {
		_selectOutput(selector);
		esp_rom_delay_us(selectorSettleDelayUs);
		for (auto const& mapping : inputPins) {
			bool value = true;
			if (!gpio.read(mapping.pin, value) || value) {
				continue;
			}

			auto const position = keyPositionForScan(selector, mapping);
			state |= _keyBit(position.row, position.column);
		}
	}

	_selectOutput(selectorIdleValue);
	return state;
}

void CardputerKeyboard::_selectOutput(std::uint8_t selector) const {
	auto const& gpio = _platformServices.gpio;
	if (gpio.write == nullptr) {
		return;
	}

	selector &= 0x07U;
	for (std::uint8_t bit = 0; bit < 3; ++bit) {
		gpio.write(selectorPins[bit], (selector & (1U << bit)) != 0);
	}
}

void CardputerKeyboard::_applyStableState(std::uint64_t state) {
	auto const changed = _stableState ^ state;
	if (changed == 0) {
		return;
	}

	auto const previousState = _stableState;
	_stableState = state;
	_stableModifiers = _modifiersForState(_stableState);
	_updatePressedKeyCache(_stableState);

	auto const now = _uptimeMs();
	for (std::uint8_t row = 0; row < rowCount; ++row) {
		for (std::uint8_t column = 0; column < columnCount; ++column) {
			auto const bit = _keyBit(row, column);
			if ((changed & bit) == 0) {
				continue;
			}

			auto const& key = _keyAt(row, column);
			if (key.modifier != Modifier::None) {
				continue;
			}

			_pushEvent(_isPressed(state, row, column) ? InputEventType::KeyDown : InputEventType::KeyUp, row, column, now);
		}
	}

	(void) previousState;
}

void CardputerKeyboard::_updatePressedKeyCache(std::uint64_t state) {
	_pressedKeys.fill(false);
	for (std::uint8_t row = 0; row < rowCount; ++row) {
		for (std::uint8_t column = 0; column < columnCount; ++column) {
			if (!_isPressed(state, row, column)) {
				continue;
			}
			auto const index = _keyIndex(_keyAt(row, column).key);
			if (index < _pressedKeys.size()) {
				_pressedKeys[index] = true;
			}
		}
	}
}

void CardputerKeyboard::_pushEvent(InputEventType type, std::uint8_t row, std::uint8_t column, std::uint32_t timestampMs) {
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

bool CardputerKeyboard::_popEvent(InputEvent& event) {
	if (_eventCount == 0) {
		return false;
	}
	event = _events[_eventTail];
	_eventTail = (_eventTail + 1) % _events.size();
	--_eventCount;
	return true;
}

std::uint8_t CardputerKeyboard::_modifiersForState(std::uint64_t state) const {
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

std::uint8_t CardputerKeyboard::_scanCode(std::uint8_t row, std::uint8_t column) {
	return static_cast<std::uint8_t>(row * columnCount + column);
}

std::uint64_t CardputerKeyboard::_keyBit(std::uint8_t row, std::uint8_t column) {
	return 1ULL << _scanCode(row, column);
}

bool CardputerKeyboard::_isPressed(std::uint64_t state, std::uint8_t row, std::uint8_t column) {
	return (state & _keyBit(row, column)) != 0;
}

CardputerKeyboard::KeyDefinition const& CardputerKeyboard::_keyAt(std::uint8_t row, std::uint8_t column) {
	return keyMap[row][column];
}

std::uint32_t CardputerKeyboard::_keyIndex(KeyCode key) {
	return static_cast<std::uint32_t>(key);
}

std::uint32_t CardputerKeyboard::_uptimeMs() const {
	auto const& clock = _platformServices.clock;
	return clock.uptimeMs != nullptr ? clock.uptimeMs() : 0U;
}

} // namespace lexxos::devices
