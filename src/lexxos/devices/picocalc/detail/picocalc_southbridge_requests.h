#pragma once

#include <cstdint>

#include <lexxos/devices/picocalc/detail/picocalc_southbridge_command_tracker.h>
#include <lexxos/devices/picocalc/detail/picocalc_southbridge_snapshot_cache.h>
#include <lexxos/devices/picocalc/picocalc_southbridge.h>

namespace lexxos::devices::detail {

class PicoCalcSouthbridgeRequests final {
public:
	[[nodiscard]] static bool requestLcdBacklight(
	    PicoCalcSouthbridge const& southbridge,
	    PicoCalcSouthbridgeSnapshotCache& snapshots,
	    PicoCalcSouthbridgeCommandTracker& commands,
	    std::uint8_t value,
	    std::uint32_t nowMs);

	[[nodiscard]] static bool requestKeyboardBacklight(
	    PicoCalcSouthbridge const& southbridge,
	    PicoCalcSouthbridgeSnapshotCache& snapshots,
	    PicoCalcSouthbridgeCommandTracker& commands,
	    std::uint8_t value,
	    std::uint32_t nowMs);

	[[nodiscard]] static bool requestReset(
	    PicoCalcSouthbridge const& southbridge,
	    PicoCalcSouthbridgeCommandTracker& commands,
	    std::uint8_t delaySeconds,
	    std::uint32_t nowMs);

	[[nodiscard]] static bool requestPowerOff(
	    PicoCalcSouthbridge const& southbridge,
	    PicoCalcSouthbridgeCommandTracker& commands,
	    std::uint32_t nowMs);
};

} // namespace lexxos::devices::detail
