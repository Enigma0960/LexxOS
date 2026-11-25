#pragma once

#include <cstdint>
#include <span>

namespace lexxos::devices {

// Три байта на пиксель ёмкости независимо от выбранного wire format.
struct Ili9488Buffers {
	std::span<std::uint8_t> pixels;
	std::span<std::uint8_t> repeated;
	bool valid() const;
};

} // namespace lexxos::devices
