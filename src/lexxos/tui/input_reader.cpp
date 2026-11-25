#include <lexxos/tui/input_reader.h>

#include <lexxos/app/detail/picocalc_lcd_console_font.h>
#include <lexxos/devices/display_device.h>

namespace lexxos::tui {

namespace {

	bool pressed(bool previous, bool current) {
		return current && !previous;
	}

} // namespace

InputReader::InputReader(lexxos::devices::GamepadDevice* gamepad, app::ProgramInputService& input)
    : _input(input)
    , _gamepad(gamepad) {
}

void InputReader::reset() {
	_input.reset(_inputCursor);
	_previousGamepad = _gamepadState();
	_ready = true;
}

bool InputReader::poll(Action& action) {
	if (!_ready) {
		reset();
	}

	Action inputAction = Action::None;
	lexxos::devices::InputEvent event{};
	while (_input.pollEvent(_inputCursor, event)) {
		inputAction = _actionForInputEvent(event);
		if (inputAction != Action::None) {
			break;
		}
	}

	auto const currentGamepad = _gamepadState();
	auto const gamepadAction = _actionForGamepadEdge(_previousGamepad, currentGamepad);
	_previousGamepad = currentGamepad;

	if (gamepadAction == Action::Start || gamepadAction == Action::Select) {
		action = gamepadAction;
		return true;
	}

	if (inputAction != Action::None) {
		action = inputAction;
		return true;
	}

	if (gamepadAction != Action::None) {
		action = gamepadAction;
		return true;
	}

	action = Action::None;
	return false;
}

lexxos::devices::GamepadState InputReader::_gamepadState() const {
	return _gamepad != nullptr && _gamepad->isReady() ? _gamepad->state() : lexxos::devices::GamepadState{};
}

Action InputReader::_actionForInputEvent(lexxos::devices::InputEvent const& event) {
	if (event.type != lexxos::devices::InputEventType::KeyDown) {
		return Action::None;
	}

	using lexxos::devices::KeyCode;
	switch (event.key) {
		case KeyCode::Up:
			return Action::Up;
		case KeyCode::Down:
			return Action::Down;
		case KeyCode::Left:
			return Action::Left;
		case KeyCode::Right:
			return Action::Right;
		case KeyCode::Enter:
		case KeyCode::Space:
		case KeyCode::A    :
		case KeyCode::Z    :
		case KeyCode::Num1:
			return Action::Confirm;
		case KeyCode::Escape:
		case KeyCode::B     :
		case KeyCode::X     :
		case KeyCode::Num3:
			return Action::Back;
		case KeyCode::Tab:
			return Action::Start;
		case KeyCode::Backspace:
			return Action::Select;
		default:
			return Action::None;
	}
}

Action InputReader::_actionForGamepadEdge(
    lexxos::devices::GamepadState const& previous,
    lexxos::devices::GamepadState const& current) {
	if (pressed(previous.up, current.up)) {
		return Action::Up;
	}

	if (pressed(previous.down, current.down)) {
		return Action::Down;
	}

	if (pressed(previous.left, current.left)) {
		return Action::Left;
	}

	if (pressed(previous.right, current.right)) {
		return Action::Right;
	}

	if (pressed(previous.a, current.a) || pressed(previous.x, current.x)) {
		return Action::Confirm;
	}

	if (pressed(previous.b, current.b) || pressed(previous.y, current.y)) {
		return Action::Back;
	}

	if (pressed(previous.start, current.start)) {
		return Action::Start;
	}

	if (pressed(previous.select, current.select)) {
		return Action::Select;
	}

	return Action::None;
}

} // namespace lexxos::tui
