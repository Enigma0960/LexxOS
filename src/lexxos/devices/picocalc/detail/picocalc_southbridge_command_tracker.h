#pragma once

#include <cstdint>

#include <lexxos/devices/picocalc/picocalc_southbridge.h>

namespace lexxos::devices::detail {

class PicoCalcSouthbridgeCommandTracker final {
public:
	void reset();
	void start(PicoCalcSouthbridgeCommandKind kind, std::uint8_t requestedValue, std::uint32_t nowMs);
	void finish(PicoCalcSouthbridgeCommandState state, std::uint32_t nowMs);
	void finish(PicoCalcSouthbridgeCommandState state, std::uint8_t acceptedValue, std::uint32_t nowMs);

	[[nodiscard]] PicoCalcSouthbridgeCommandStatus const& last() const;

private:
	PicoCalcSouthbridgeCommandStatus _last{};
};

} // namespace lexxos::devices::detail
