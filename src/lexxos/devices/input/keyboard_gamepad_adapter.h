#pragma once

#include <string_view>

#include <lexxos/devices/gamepad_device.h>
#include <lexxos/devices/input_device.h>

namespace lexxos::devices {

struct GamepadKeyMap {
	KeyCode up{KeyCode::Up};
	KeyCode down{KeyCode::Down};
	KeyCode left{KeyCode::Left};
	KeyCode right{KeyCode::Right};
	KeyCode a{KeyCode::A};
	KeyCode b{KeyCode::S};
	KeyCode x{KeyCode::Z};
	KeyCode y{KeyCode::X};
	KeyCode start{KeyCode::Enter};
	KeyCode select{KeyCode::Backspace};
};

class KeyboardGamepadAdapter final : public GamepadDevice {
public:
	KeyboardGamepadAdapter(std::string_view name, InputDevice& input, GamepadKeyMap map = {}, bool updateBeforeRead = true);

	[[nodiscard]] std::string_view name() const override;
	[[nodiscard]] bool isReady() const override;
	[[nodiscard]] GamepadState state() override;

private:
	std::string_view _name;
	InputDevice& _input;
	GamepadKeyMap _map;

	bool _updateBeforeRead{true};
};

} // namespace lexxos::devices
