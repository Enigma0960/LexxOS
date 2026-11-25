#pragma once
#include <cstdint>

namespace lexxos::devices {

enum class SpiClockMode : std::uint8_t {
	Mode0 = 0,
	Mode1 = 1,
	Mode2 = 2,
	Mode3 = 3,
};

} // namespace lexxos::devices
