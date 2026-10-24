#pragma once

#include <cstdint>

#include <lexxos/devices/picocalc/picocalc_southbridge.h>

namespace lexxos::platform {
struct PlatformClockService;
} // namespace lexxos::platform

namespace lexxos::devices::input_detail {

class PicoCalcKeyboardTransport final {
public:
	explicit PicoCalcKeyboardTransport(PicoCalcSouthbridge& southbridge);

	[[nodiscard]] bool readState(PicoCalcKeyboardState& state) const;
	[[nodiscard]] bool readStateWithRetry(
	    PicoCalcKeyboardState& state,
	    std::uint8_t retryCount,
	    std::uint32_t retryDelayMs,
	    lexxos::platform::PlatformClockService const& clock) const;
	[[nodiscard]] bool readFifo(PicoCalcKeyboardFifoEvent& event) const;
	[[nodiscard]] bool drainFifo(std::uint8_t maxEvents) const;

private:
	static void _sleepMs(lexxos::platform::PlatformClockService const& clock, std::uint32_t delayMs);

	PicoCalcSouthbridge& _southbridge;
};

} // namespace lexxos::devices::input_detail
