#pragma once

#include <cstdint>

namespace lexxos::platform::stm32::detail {

struct DecodedPin {
	char port{'?'};
	std::uint32_t bit{0};
	std::uint32_t base{0};
};

} // namespace lexxos::platform::stm32::detail
