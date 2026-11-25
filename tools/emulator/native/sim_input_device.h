#pragma once

#include <string_view>

#include <lexxos/devices/input_device.h>

namespace lexxos::emulator {

class SimInputDevice final : public lexxos::devices::InputDevice {
public:
	[[nodiscard]] std::string_view name() const override;
	[[nodiscard]] bool isReady() const override;

	bool pollEvent(lexxos::devices::InputEvent& event) override;
	[[nodiscard]] bool isPressed(lexxos::devices::KeyCode key) const override;
};

[[nodiscard]] SimInputDevice& simInputDevice();

} // namespace lexxos::emulator
