#pragma once

#include <cstdint>

#include <lexxos/devices/input/detail/picocalc_keyboard_transport.h>
#include <lexxos/devices/picocalc/picocalc_southbridge_service.h>

namespace lexxos::platform {
struct PlatformClockService;
} // namespace lexxos::platform

namespace lexxos::devices::input_detail {

struct PicoCalcKeyboardInitializeResult {
	bool stateReadOk{false};
	bool drainOk{true};
	PicoCalcKeyboardState state{};
	std::uint8_t pendingFifoCount{0};
};

class PicoCalcKeyboardInitializer final {
public:
	[[nodiscard]] PicoCalcKeyboardInitializeResult initialize(
	    PicoCalcKeyboardTransport const& transport,
	    PicoCalcSouthbridgeService& service,
	    lexxos::platform::PlatformClockService const& clock) const;

private:
	static constexpr std::uint8_t retryCount = 10;
	static constexpr std::uint32_t retryDelayMs = 20;
	static constexpr std::uint8_t drainEventLimit = 16;

	[[nodiscard]] static std::uint32_t _uptimeMs(lexxos::platform::PlatformClockService const& clock);
};

} // namespace lexxos::devices::input_detail
