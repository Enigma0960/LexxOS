#pragma once

#include <cstdint>

namespace lexxos::apps::utility_programs::detail {

struct BacklightSettings {
	std::uint8_t lcd{0};
	std::uint8_t keyboard{0};
	bool hasLcd{false};
	bool hasKeyboard{false};
};

enum class BacklightSelection : std::uint8_t {
	All,
	Lcd,
	Keyboard,
	Invalid,
};

} // namespace lexxos::apps::utility_programs::detail
