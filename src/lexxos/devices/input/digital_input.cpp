#include <lexxos/devices/input/digital_input.h>

namespace lexxos::devices {

DigitalInput::DigitalInput(DigitalInputConfig config, platform::PlatformGpioService const& gpio)
    : _config(config)
    , _gpio(gpio) {
}

bool DigitalInput::pressed() const {
	return _high == (_config.activeLevel == platform::GpioActiveLevel::High);
}

void DigitalInput::setHandler(Handler handler, void* context) {
	_handler = handler;
	_context = context;
}

bool DigitalInput::initialize(Handler handler, void* context) {
	setHandler(handler, context);
	_ready = false;
	return handler && initialize();
}

bool DigitalInput::initialize() {
	_ready = false;
	if (!_config.pin.isValid() || !_config.clickGapMs || !_config.holdMs || _config.longHoldMs <= _config.holdMs || !_gpio.configureInput || !_gpio.read) {
		return false;
	}

	if (!_gpio.configureInput(_config.pin, _config.pull) || !_gpio.read(_config.pin, _high)) {
		return false;
	}

	_rawHigh = _high;
	_ignoreUntilRelease = pressed();
	_clicks = 0;
	_held = false;
	_longHeld = false;
	_ready = true;
	return true;
}

bool DigitalInput::poll(std::uint32_t nowMs) {
	if (!_ready) {
		return false;
	}

	bool high{};
	if (!_gpio.read(_config.pin, high)) {
		_ready = false;
		return false;
	}

	if (high != _rawHigh) {
		_rawHigh = high;
		_rawSince = nowMs;
	}

	if (_rawHigh != _high) {
		// Пока фронт подтверждается, таймеры учитывают время лишь до его начала.
		advanceTimers(_rawSince);
		if (nowMs - _rawSince < _config.debounceMs) {
			return true;
		}

		acceptEdge(_rawSince);
	}

	advanceTimers(nowMs);
	return true;
}

void DigitalInput::acceptEdge(std::uint32_t edgeMs) {
	_high = _rawHigh;
	_edgeTime = edgeMs;
	emit(DigitalInputEvent::Changed);
	emit(_high ? DigitalInputEvent::BecameHigh : DigitalInputEvent::BecameLow);
	if (pressed()) {
		_held = false;
		_longHeld = false;
	} else {
		if (!_ignoreUntilRelease && !_held) {
			++_clicks;
			if (_clicks == 3) {
				finishClicks();
			}
		}

		_ignoreUntilRelease = false;
	}
}

void DigitalInput::advanceTimers(std::uint32_t nowMs) {
	auto const elapsed = nowMs - _edgeTime;
	if (!pressed()) {
		if (_clicks && elapsed >= _config.clickGapMs) {
			finishClicks();
		}

		return;
	}

	if (_ignoreUntilRelease) {
		return;
	}

	if (!_held && elapsed >= _config.holdMs) {
		if (_clicks) {
			finishClicks();
		}

		_held = true;
		emit(DigitalInputEvent::Hold);
	}

	if (!_longHeld && elapsed >= _config.longHoldMs) {
		_longHeld = true;
		emit(DigitalInputEvent::LongHold);
	}
}

void DigitalInput::finishClicks() {
	constexpr DigitalInputEvent events[]{
	    DigitalInputEvent::SingleClick, DigitalInputEvent::DoubleClick, DigitalInputEvent::TripleClick};
	auto const event = events[_clicks - 1];
	_clicks = 0;
	emit(event);
}

void DigitalInput::emit(DigitalInputEvent event) const {
	if (_handler) {
		_handler(event, _high, _context);
	}
}

} // namespace lexxos::devices
