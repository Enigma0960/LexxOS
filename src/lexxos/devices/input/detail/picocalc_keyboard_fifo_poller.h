#pragma once

#include <array>
#include <cstdint>

#include <lexxos/devices/input/detail/picocalc_keyboard_transport.h>

namespace lexxos::devices::input_detail {

struct PicoCalcKeyboardFifoPollResult {
	bool communicationOk{true};
	std::uint8_t successfulReads{0};
	std::uint8_t eventCount{0};
	std::uint8_t lastStatus{0};
};

class PicoCalcKeyboardFifoPoller final {
public:
	static constexpr std::uint8_t readLimit = 8;
	using EventBuffer = std::array<PicoCalcKeyboardFifoEvent, readLimit>;

	[[nodiscard]] PicoCalcKeyboardFifoPollResult poll(
	    PicoCalcKeyboardTransport const& transport,
	    EventBuffer& events) const;
};

} // namespace lexxos::devices::input_detail
