#pragma once

#include <lexxos/app/program_input_service.h>
#include <lexxos/devices/gamepad_device.h>
#include <lexxos/tui/tui_types.h>

namespace lexxos::tui {

class InputReader final {
public:
	explicit InputReader(lexxos::devices::GamepadDevice* gamepad, app::ProgramInputService& input);

	void reset();
	[[nodiscard]] bool poll(Action& action);

private:
	app::ProgramInputService& _input;
	lexxos::devices::GamepadDevice* _gamepad{nullptr};
	lexxos::devices::GamepadState _previousGamepad{};
	lexxos::app::program_input::EventCursor _inputCursor{};
	bool _ready{false};

	[[nodiscard]] lexxos::devices::GamepadState _gamepadState() const;
	[[nodiscard]] static Action _actionForInputEvent(lexxos::devices::InputEvent const& event);
	[[nodiscard]] static Action _actionForGamepadEdge(
	    lexxos::devices::GamepadState const& previous,
	    lexxos::devices::GamepadState const& current);
};

} // namespace lexxos::tui
