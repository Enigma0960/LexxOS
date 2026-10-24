#include <lexxos/devices/input/keyboard_gamepad_adapter.h>

namespace lexxos::devices {

KeyboardGamepadAdapter::KeyboardGamepadAdapter(std::string_view name, InputDevice& input, GamepadKeyMap map, bool updateBeforeRead)
    : _name(name)
    , _input(input)
    , _map(map)
    , _updateBeforeRead(updateBeforeRead) {
}

std::string_view KeyboardGamepadAdapter::name() const {
	return _name;
}

bool KeyboardGamepadAdapter::isReady() const {
	return _input.isReady();
}

GamepadState KeyboardGamepadAdapter::state() {
	if (_updateBeforeRead) {
		_input.update();
	}

	return {
	    .up = _input.isPressed(_map.up),
	    .down = _input.isPressed(_map.down),
	    .left = _input.isPressed(_map.left),
	    .right = _input.isPressed(_map.right),
	    .a = _input.isPressed(_map.a),
	    .b = _input.isPressed(_map.b),
	    .x = _input.isPressed(_map.x),
	    .y = _input.isPressed(_map.y),
	    .start = _input.isPressed(_map.start),
	    .select = _input.isPressed(_map.select),
	};
}

} // namespace lexxos::devices
