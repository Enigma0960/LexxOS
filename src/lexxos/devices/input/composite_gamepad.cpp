#include <lexxos/devices/input/composite_gamepad.h>

namespace lexxos::devices {

CompositeGamepad::CompositeGamepad(
    std::string_view name, GamepadDevice& first, GamepadDevice& second)
    : _name(name)
    , _first(first)
    , _second(second) {
}

std::string_view CompositeGamepad::name() const {
	return _name;
}

bool CompositeGamepad::isReady() const {
	return _first.isReady() || _second.isReady();
}

GamepadState CompositeGamepad::state() {
	GamepadState result{};
	if (_first.isReady()) {
		result = _first.state();
	}
	if (_second.isReady()) {
		auto const second = _second.state();
		result.up = result.up || second.up;
		result.down = result.down || second.down;
		result.left = result.left || second.left;
		result.right = result.right || second.right;
		result.a = result.a || second.a;
		result.b = result.b || second.b;
		result.x = result.x || second.x;
		result.y = result.y || second.y;
		result.start = result.start || second.start;
		result.select = result.select || second.select;
	}
	return result;
}

} // namespace lexxos::devices
