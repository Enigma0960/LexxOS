#pragma once

#include <cstdint>

namespace lexxos::platform::rk3506 {

// Task-context CPU0: одна регистрация общей линии для SD и LCD.
class DmaIrqClients {
public:
	using Configure = bool (*)(bool);
	explicit DmaIrqClients(Configure configure);
	bool setEnabled(unsigned client, bool enabled);

private:
	Configure _configure;
	std::uint32_t _clients{};
};

} // namespace lexxos::platform::rk3506
