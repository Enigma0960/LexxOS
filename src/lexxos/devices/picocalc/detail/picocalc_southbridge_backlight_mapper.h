#pragma once

#include <cstdint>

namespace lexxos::devices::detail {

class PicoCalcSouthbridgeBacklightMapper final {
public:
	[[nodiscard]] static std::uint8_t normalizeLcd(std::uint8_t value);
	[[nodiscard]] static std::uint8_t normalizeKeyboard(std::uint8_t value);
};

} // namespace lexxos::devices::detail
