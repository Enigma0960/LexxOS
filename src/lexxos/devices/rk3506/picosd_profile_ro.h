#pragma once
#include <cstdint>

namespace lexxos::platform::rk3506 {

// All cycle fields are CNTVCT ticks (24 MHz on RK3506). These metrics
// are cumulative and can be sampled around each benchmark phase.
struct PicoSdReadProfile {
	std::uint64_t commandCycles{}; // CMD17/CMD18 frame, select, response
	std::uint64_t tokenCycles{}; // SD data-token polling (0xFE)
	std::uint64_t crcWireCycles{}; // Two CRC bytes over the SPI bus
	std::uint64_t crcComputeCycles{}; // Software CRC16 verification
	std::uint64_t stopCycles{}; // CMD12 including busy release
	std::uint32_t cmd17{};
	std::uint32_t cmd18{};
	std::uint32_t cmd12{};
	std::uint32_t tokenPolls{};
	std::uint32_t goodSectors{};
	std::uint32_t tokenErrors{};
	std::uint32_t crcErrors{};
	std::uint32_t readErrors{};
};

// CRC algorithms must produce identical SD CRC16-CCITT (poly 0x1021,
// init 0) results. Table is the default; Bitwise remains a test reference.
enum class PicoSdCrcMode : std::uint8_t {
	Bitwise,
	Table,
	Slicing4,
};

PicoSdCrcMode picoSdCrcMode();
bool picoSdSetCrcMode(PicoSdCrcMode mode);
PicoSdReadProfile picoSdReadProfileStatistics();
bool picoSdResetReadProfileStatistics();

} // namespace lexxos::platform::rk3506
