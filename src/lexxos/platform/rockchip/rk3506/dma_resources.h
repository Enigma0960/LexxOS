#pragma once

#include <cstdint>

namespace lexxos::platform::rk3506::dma {

// Fixed DMAC0 allocation for the current SPI backends. These declarations do
// not grant runtime ownership: each client must hold its SPI transaction.
inline constexpr std::uint32_t sdChannels = (1U << 0U) | (1U << 1U);
inline constexpr std::uint32_t sdEvents = (1U << 0U) | (1U << 1U);
inline constexpr std::uint32_t lcdChannel = 2U;
inline constexpr std::uint8_t lcdEvent = 2U;
inline constexpr std::uint8_t lcdTxRequest = 0U;
inline constexpr std::uint32_t lcdChannels = 1U << lcdChannel;
inline constexpr std::uint32_t lcdEvents = 1U << lcdEvent;
static_assert((sdChannels & lcdChannels) == 0U && (sdEvents & lcdEvents) == 0U);

// The manager is shared; channel faults belong only to the declared client.
constexpr bool hasFault(std::uint32_t manager, std::uint32_t channels, std::uint32_t ownedChannels) {
	return (manager & 1U) != 0U || (channels & ownedChannels) != 0U;
}

} // namespace lexxos::platform::rk3506::dma
