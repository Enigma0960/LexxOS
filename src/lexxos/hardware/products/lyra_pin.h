#pragma once

#include <cstdint>
#include <optional>

namespace lexxos::hardware::products {

// The current binding spans two different pin namespaces. Conversion to the
// legacy integer representation is confined to the LegacyBoardDescriptor adapter.
enum class LyraPinDomain : std::uint8_t {
	Gpio0,
	Gpio1,
	RmIo,
};

struct LyraPin {
	LyraPinDomain domain;
	int number;

	constexpr bool operator==(LyraPin const&) const = default;
};

// Resolve only aliases established by the existing LCD/I2C backends:
// picocalc_lcd_spi0.cpp and picocalc_i2c_bus.cpp. Unknown RM_IO mappings fail
// closed; numerical equality is not evidence of a physical connection.
[[nodiscard]] constexpr std::optional<int> lyraGpio0Pin(LyraPin pin) {
	if (pin.domain == LyraPinDomain::Gpio0 && pin.number >= 0 && pin.number < 32) {
		return pin.number;
	}

	if (pin.domain == LyraPinDomain::RmIo) {
		switch (pin.number) {
			case 6 :
			case 7 :
			case 10:
			case 11:
				return pin.number;
			default:
				break;
		}
	}

	return std::nullopt;
}

} // namespace lexxos::hardware::products
