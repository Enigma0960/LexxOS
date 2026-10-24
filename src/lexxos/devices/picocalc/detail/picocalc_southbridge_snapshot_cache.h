#pragma once

#include <cstdint>

#include <lexxos/devices/picocalc/picocalc_southbridge.h>

namespace lexxos::devices::detail {

class PicoCalcSouthbridgeSnapshotCache final {
public:
	static constexpr std::uint32_t defaultRefreshIntervalMs = 1000;

	[[nodiscard]] bool refresh(PicoCalcSouthbridge const& southbridge, std::uint32_t nowMs);
	[[nodiscard]] bool refreshDue(std::uint32_t nowMs) const;
	[[nodiscard]] bool refreshIfDue(PicoCalcSouthbridge const& southbridge, std::uint32_t nowMs);

	void publishKeyboardState(PicoCalcKeyboardState const& state, std::uint32_t nowMs);
	void publishKeyboardState(std::uint8_t state, std::uint8_t value, std::uint32_t nowMs);
	void invalidate();

	void publishLcdBacklight(std::uint8_t value, std::uint32_t nowMs);
	void publishKeyboardBacklight(std::uint8_t value, std::uint32_t nowMs);
#if defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
	void publishJcsRtcConfig(PicoCalcJcsRtcConfig config, std::uint32_t nowMs);
	void publishJcsRtcDateTime(lexxos::devices::rtc::DateTime const& value, std::uint32_t nowMs);
	void publishJcsRtcAlarm(lexxos::devices::rtc::DateTime const& value, std::uint32_t nowMs);
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

private:
	void _publishSuccessfulPartialRefresh(std::uint32_t nowMs);

	PicoCalcSouthbridgeSnapshot _snapshot{};
	std::uint32_t _refreshIntervalMs{defaultRefreshIntervalMs};
	std::uint32_t _lastRefreshMs{0};
	std::uint32_t _lastFullRefreshMs{0};
	std::uint32_t _successfulRefreshes{0};
	std::uint32_t _failedRefreshes{0};
	bool _hasSnapshot{false};
	bool _lastRefreshOk{false};
	bool _hasFullRefresh{false};
};

} // namespace lexxos::devices::detail
