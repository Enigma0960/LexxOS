#pragma once

#include <lexxos/devices/device.h>

namespace lexxos::devices {

struct GamepadState {
	bool up{false};
	bool down{false};
	bool left{false};
	bool right{false};
	bool a{false};
	bool b{false};
	bool x{false};
	bool y{false};
	bool start{false};
	bool select{false};
};

class GamepadDevice : public Device {
public:
	[[nodiscard]] DeviceKind kind() const override;

	[[nodiscard]] virtual GamepadState state() = 0;
};

} // namespace lexxos::devices
