#pragma once

#include <cstdint>

namespace lexxos::platform::rk3506 {

enum class Dmac0Client {
	Sd,
	Lcd
};

enum class Dmac0RouteResult {
	Ready,
	Conflict,
	WriteFailed,
	InvalidClient
};

// Clock is enable-only: an individual client must never gate/reset shared DMAC0.
void dmac0EnableClock();
// Probe-time only, with the client's SPI transaction held and channels idle.
Dmac0RouteResult dmac0ConfigureSpiRoute(Dmac0Client client);
// CPU0 critical RMW; preserves every other client's INTEN bits.
void dmac0SetEventsEnabled(Dmac0Client client, bool enabled);
// IRQ-safe W1C: acknowledges only the intersection with this client's events.
void dmac0Acknowledge(Dmac0Client client, std::uint32_t pending = UINT32_MAX);
// Disable own events, kill own channels, wait for idle, acknowledge own events.
// Each debug submission and the final idle wait have separate 20 ms limits.
// False means submission/stop/fault recovery failed; no shared manager reset.
// The driver must keep itself failed until a successful reprobe, even on true.
bool dmac0Abort(Dmac0Client client);

// Shared DMAC0 debug-port owner. Caller holds its SPI transaction; these calls
// serialize only command submission, not the lifetime of the DMA transfer.
bool dmac0StartNonsecure(Dmac0Client client, unsigned channel, std::uint32_t programAddress);
bool dmac0Kill(Dmac0Client client, unsigned channel);

} // namespace lexxos::platform::rk3506
