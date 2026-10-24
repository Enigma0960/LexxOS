#pragma once

#include <cstdint>

namespace lexxos::devices::input::detail {

struct SegaGenesisGamepadSample {
	std::uint16_t highPhase{0xFFFF};
	std::uint16_t lowPhase{0xFFFF};
};

} // namespace lexxos::devices::input::detail
