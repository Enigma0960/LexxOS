#include <lexxos/devices/espidf/m5stack_buttons_input_device.h>

#include <lexxos/platform/platform_services.h>

namespace lexxos::devices {
namespace {

	constexpr M5StackButtonConfig defaultButtons[] = {
	    {
         .pin = {39},
         .key = KeyCode::A,
         .activeLevel = lexxos::platform::GpioActiveLevel::Low,
	     },
	    {
         .pin = {38},
         .key = KeyCode::B,
         .activeLevel = lexxos::platform::GpioActiveLevel::Low,
	     },
	    {
         .pin = {37},
         .key = KeyCode::C,
         .activeLevel = lexxos::platform::GpioActiveLevel::Low,
	     },
	};

	[[nodiscard]] bool isActive(bool gpioValue, lexxos::platform::GpioActiveLevel activeLevel) {
		return activeLevel == lexxos::platform::GpioActiveLevel::High ? gpioValue : !gpioValue;
	}

} // namespace

M5StackButtonsInputDevice::M5StackButtonsInputDevice(M5StackButtonsInputConfig config)
    : M5StackButtonsInputDevice(config, lexxos::platform::services()) {
}

M5StackButtonsInputDevice::M5StackButtonsInputDevice(
    M5StackButtonsInputConfig config,
    lexxos::platform::PlatformServices const& platform)
    : _platformServices(platform)
    , _config(config) {
	if (_config.buttons == nullptr || _config.buttonCount == 0) {
		_config.buttons = defaultButtons;
		_config.buttonCount = std::size(defaultButtons);
	}
}

bool M5StackButtonsInputDevice::initialize() {
	if (!_isConfigurationValid()) {
		_ready = false;
		return false;
	}

	auto const& gpio = _platformServices.gpio;
	if (gpio.configureInput == nullptr) {
		_ready = false;
		return false;
	}

	for (std::size_t index = 0; index < _config.buttonCount; ++index) {
		if (!gpio.configureInput(_config.buttons[index].pin, lexxos::platform::GpioPull::None)) {
			_ready = false;
			return false;
		}
	}

	_ready = true;
	_eventHead = 0;
	_eventTail = 0;
	_eventCount = 0;
	_lastRawState = _readRawState();
	_stableState = _lastRawState;
	_pressedKeys = _pressedKeyMask(_stableState);
	_lastRawChangeMs = _uptimeMs();
	return true;
}

void M5StackButtonsInputDevice::update() {
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

	if (rawState != _stableState && now - _lastRawChangeMs >= _config.debounceMs) {
		_applyStableState(rawState);
	}
}

std::string_view M5StackButtonsInputDevice::name() const {
	return _config.name;
}

bool M5StackButtonsInputDevice::isReady() const {
	return _ready;
}

bool M5StackButtonsInputDevice::pollEvent(InputEvent& event) {
	update();
	return _popEvent(event);
}

bool M5StackButtonsInputDevice::isPressed(KeyCode key) const {
	auto const mask = _keyMask(key);
	return mask != 0 && (_pressedKeys & mask) != 0;
}

std::uint32_t M5StackButtonsInputDevice::rawState() const {
	return _stableState;
}

std::uint32_t M5StackButtonsInputDevice::rawKeyCount() const {
	return static_cast<std::uint32_t>(_config.buttonCount);
}

bool M5StackButtonsInputDevice::_isConfigurationValid() const {
	if (_config.name.empty() || _config.buttons == nullptr || _config.buttonCount == 0 || _config.buttonCount > maxButtons) {
		return false;
	}
	for (std::size_t index = 0; index < _config.buttonCount; ++index) {
		if (!_config.buttons[index].pin.isValid() || _config.buttons[index].key == KeyCode::Unknown) {
			return false;
		}
	}
	return true;
}

std::uint32_t M5StackButtonsInputDevice::_readRawState() const {
	std::uint32_t state = 0;
	for (std::size_t index = 0; index < _config.buttonCount; ++index) {
		bool value = false;
		auto const& gpio = _platformServices.gpio;
		if (gpio.read == nullptr || !gpio.read(_config.buttons[index].pin, value)) {
			continue;
		}
		if (isActive(value, _config.buttons[index].activeLevel)) {
			state |= (1U << index);
		}
	}
	return state;
}

void M5StackButtonsInputDevice::_applyStableState(std::uint32_t state) {
	auto const changed = _stableState ^ state;
	if (changed == 0) {
		return;
	}

	auto const now = _uptimeMs();
	for (std::size_t index = 0; index < _config.buttonCount; ++index) {
		auto const bit = 1U << index;
		if ((changed & bit) == 0) {
			continue;
		}

		auto const key = _config.buttons[index].key;
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

void M5StackButtonsInputDevice::_pushEvent(InputEventType type, KeyCode key, std::uint32_t timestampMs) {
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

bool M5StackButtonsInputDevice::_popEvent(InputEvent& event) {
	if (_eventCount == 0) {
		return false;
	}
	event = _events[_eventTail];
	_eventTail = (_eventTail + 1) % _events.size();
	--_eventCount;
	return true;
}

std::uint32_t M5StackButtonsInputDevice::_uptimeMs() const {
	auto const& clock = _platformServices.clock;
	return clock.uptimeMs != nullptr ? clock.uptimeMs() : 0U;
}

std::uint32_t M5StackButtonsInputDevice::_pressedKeyMask(std::uint32_t rawState) const {
	std::uint32_t mask = 0;
	for (std::size_t index = 0; index < _config.buttonCount; ++index) {
		if ((rawState & (1U << index)) != 0) {
			mask |= _keyMask(_config.buttons[index].key);
		}
	}
	return mask;
}

std::uint32_t M5StackButtonsInputDevice::_keyMask(KeyCode key) {
	auto const index = static_cast<std::uint32_t>(key);
	return index < 32 ? (1U << index) : 0U;
}

} // namespace lexxos::devices
