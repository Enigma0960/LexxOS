#pragma once

#include <cstddef>
#include <cstdint>

namespace lexxos::platform::rk3506 {

// Dedicated SPI0 output for the PicoCalc LCD; SPI1 belongs to microSD.
// Lyra defaults to PL330 RGB565 direct16; packed8/PIO remain fallbacks.
// Накопительные счётчики CPU0; чтение из task context, без сброса.
// ticks — время внутри синхронной записи, включая ожидание FIFO/DMA и IRQ.
struct LcdSpi0Statistics {
	std::uint64_t calls{};
	std::uint64_t requestedBytes{};
	std::uint64_t completedBytes{};
	std::uint64_t elapsedTicks{};
	std::uint64_t failures{};
	std::uint32_t counterHz{};
};

LcdSpi0Statistics lcdSpi0Statistics();

bool lcdSpi0Initialize(std::uint32_t clockHz);
bool lcdSpi0SetClockHz(std::uint32_t clockHz);
std::uint32_t lcdSpi0ClockHz();
bool lcdSpi0Write(std::uint8_t const* data, std::size_t count);

// Hardware-validated on PicoCalc Lyra: use DFS16 for native RGB565 DMA pixels.
// SPI0 returns to DFS8 for commands and other transfers.
bool lcdSpi0SetRgb565Direct16(bool enabled);
bool lcdSpi0Rgb565Direct16Enabled();
bool lcdSpi0WriteRgb565Words(std::uint16_t const* pixels, std::size_t count);

} // namespace lexxos::platform::rk3506
