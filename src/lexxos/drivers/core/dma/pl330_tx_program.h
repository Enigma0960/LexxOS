#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace lexxos::drivers::pl330 {

// Physical bus addresses and controller-local resource IDs supplied by the owner.
// This encoder does not acquire channels, configure clocks/IRQs or access memory.
struct TxTransfer {
	std::uint32_t source{};
	std::uint32_t destination{};
	std::uint32_t bytes{};
	std::uint8_t unitBytes{1};
	std::uint8_t request{};
	std::uint8_t event{};
	std::uint8_t burstUnits{1}; // Source/destination transfers per DMAWFP burst.
};

inline constexpr std::size_t txProgramCapacity = 256;
inline constexpr std::uint32_t maxTxBytes = 320U * 320U * 3U;

// Non-secure, incrementing source, fixed peripheral destination.
// burstUnits is 1 (legacy), 2 or 4 and must divide the element count.
// Returns zero without modifying output on invalid input or insufficient storage.
// Success ends with DMAWMB -> DMASEV -> DMAEND; caller owns program lifetime.
std::size_t buildTxProgram(std::span<std::uint8_t> output, TxTransfer const& transfer);

} // namespace lexxos::drivers::pl330
