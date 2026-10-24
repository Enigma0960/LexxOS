#pragma once

#include <cstdint>

#include <lexxos/devices/picocalc/detail/picocalc_southbridge_command_tracker.h>
#include <lexxos/devices/picocalc/detail/picocalc_southbridge_snapshot_cache.h>
#include <lexxos/devices/picocalc/picocalc_southbridge.h>

namespace lexxos::devices {

class PicoCalcSouthbridgeService final {
public:
	static constexpr std::uint32_t defaultRefreshIntervalMs =
	    detail::PicoCalcSouthbridgeSnapshotCache::defaultRefreshIntervalMs;

	explicit PicoCalcSouthbridgeService(PicoCalcSouthbridge& southbridge);

	[[nodiscard]] bool refresh(std::uint32_t nowMs);
	[[nodiscard]] bool refreshDue(std::uint32_t nowMs) const;
	[[nodiscard]] bool refreshIfDue(std::uint32_t nowMs);
	void publishKeyboardState(PicoCalcKeyboardState const& state, std::uint32_t nowMs);
	void publishKeyboardState(std::uint8_t state, std::uint8_t value, std::uint32_t nowMs);
	void invalidate();
	[[nodiscard]] bool requestLcdBacklight(std::uint8_t value, std::uint32_t nowMs);
	[[nodiscard]] bool requestKeyboardBacklight(std::uint8_t value, std::uint32_t nowMs);
	[[nodiscard]] bool requestReset(std::uint8_t delaySeconds, std::uint32_t nowMs);
	[[nodiscard]] bool requestPowerOff(std::uint32_t nowMs);

#if defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
	[[nodiscard]] bool readJcsFirmwareInfo(PicoCalcJcsFirmwareInfo& info) const;
	[[nodiscard]] bool readJcsRtcConfig(PicoCalcJcsRtcConfig& config) const;
	[[nodiscard]] bool writeJcsRtcConfig(PicoCalcJcsRtcConfig config, std::uint32_t nowMs);
	[[nodiscard]] bool readJcsRtcDateTime(lexxos::devices::rtc::DateTime& value) const;
	[[nodiscard]] bool writeJcsRtcDateTime(lexxos::devices::rtc::DateTime const& value, std::uint32_t nowMs);
	[[nodiscard]] bool readJcsRtcAlarm(lexxos::devices::rtc::DateTime& value) const;
	[[nodiscard]] bool writeJcsRtcAlarm(lexxos::devices::rtc::DateTime const& value, std::uint32_t nowMs);
#endif

	void setRefreshIntervalMs(std::uint32_t intervalMs);
	[[nodiscard]] std::uint32_t refreshIntervalMs() const;

	[[nodiscard]] bool hasSnapshot() const;
	[[nodiscard]] PicoCalcSouthbridgeSnapshot const& snapshot() const;
	[[nodiscard]] bool lastRefreshOk() const;
	[[nodiscard]] std::uint32_t lastRefreshMs() const;
	[[nodiscard]] std::uint32_t lastFullRefreshMs() const;
	[[nodiscard]] std::uint32_t successfulRefreshes() const;
	[[nodiscard]] std::uint32_t failedRefreshes() const;
	[[nodiscard]] PicoCalcSouthbridgeCommandStatus lastCommandStatus() const;

private:
	PicoCalcSouthbridge& _southbridge;
	detail::PicoCalcSouthbridgeSnapshotCache _snapshots{};
	detail::PicoCalcSouthbridgeCommandTracker _commands{};
};

} // namespace lexxos::devices
