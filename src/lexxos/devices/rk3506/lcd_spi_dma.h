#pragma once

#include <cstddef>
#include <cstdint>

namespace lexxos::platform::rk3506 {

// Borrowed dependency: must outlive SPI0. Writes are synchronous, including
// failure: borrowed input may not be released while DMA can still read it.
// No retry through PIO is permitted after a failed/partial DMA write.
class LcdSpiDma {
public:
	virtual ~LcdSpiDma() = default;
	virtual bool sourceRetained() const = 0;
	virtual bool enabled() = 0;
	virtual bool write(std::uint8_t const* source, std::size_t count) = 0;
	virtual bool write16(std::uint16_t const* source, std::size_t pixels) = 0;
};

} // namespace lexxos::platform::rk3506
