#pragma once

#include <cstddef>
#include <cstdint>

namespace lexxos::platform::rk3506 {

// Future SPI1 DMA transport: expose capability independently of the PIO path.
// Requires verified DMAC0 channels, cache-safe buffers and paired TX/RX.
struct PicoSpi1DmaInfo {
	bool available{};
	std::uint32_t txRequest{2U}; // RK3506 DMAC0 SPI1 TX request
	std::uint32_t rxRequest{3U}; // RK3506 DMAC0 SPI1 RX request
	char const* reason{"no bare-metal DMAC0 SPI1 driver"};
};

PicoSpi1DmaInfo picoSpi1DmaInfo();

// The counters describe the read-only, full-duplex SPI1 FIFO path.
// Reset on every SPI1 initialization; they may wrap after prolonged use.
struct PicoSpi1FifoStats {
	std::uint32_t reads{};
	std::uint32_t bytes{};
	std::uint32_t txBursts{};
	std::uint32_t rxPolls{};
	std::uint32_t emptyRxPolls{};
	std::uint32_t rxDrainBursts{};
	std::uint32_t irqChecks{};
	std::uint32_t txLevelChecks{};
	std::uint32_t peakPending{};
	std::uint32_t peakRxFifo{};
	std::uint32_t timeouts{};
	std::uint32_t overflows{};
	std::uint32_t syncErrors{};
	std::uint32_t lastStatus{};
	std::uint32_t lastRawIrq{};
	std::uint64_t payloadCycles{}; // Sum of successful FIFO payload transfer time, CNTVCT ticks.
};

// SAFE retains the validated 9g hardware checks. FAST skips redundant
// TXFLR reads and samples sticky error flags less often at 6 MHz.
enum class PicoSpi1FifoMode : std::uint8_t {
	Safe,
	Fast
};

bool picoSpi1Initialize();
void picoSpi1Stop();
bool picoSpi1Transfer(std::uint8_t tx, std::uint8_t& rx);
bool picoSpi1ReadData(std::uint8_t* destination, std::size_t count);
void picoSpi1FastClock();
std::uint32_t picoSpi1Version();
std::uint32_t picoSpi1Divider();
std::uint32_t picoSpi1ClockMHz(); // 6 or 12 MHz after card initialization.
bool picoSpi1SetClockMHz(std::uint32_t mhz); // Only when SPI1 is idle and CS is high.
PicoSpi1FifoStats picoSpi1FifoStatistics();
PicoSpi1FifoMode picoSpi1FifoMode();
bool picoSpi1SetFifoMode(PicoSpi1FifoMode mode);
bool picoSpi1ResetFifoStatistics();

} // namespace lexxos::platform::rk3506
