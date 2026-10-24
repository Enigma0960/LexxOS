#pragma once

#include <string_view>

#include <lexxos/devices/gamepad_device.h>

namespace lexxos::devices {

class CompositeGamepad final : public GamepadDevice {
public:
	CompositeGamepad(std::string_view name, GamepadDevice& first, GamepadDevice& second);

	[[nodiscard]] std::string_view name() const override;
	[[nodiscard]] bool isReady() const override;
	[[nodiscard]] GamepadState state() override;

private:
	std::string_view _name;
	GamepadDevice& _first;
	GamepadDevice& _second;
};

} // namespace lexxos::devices
