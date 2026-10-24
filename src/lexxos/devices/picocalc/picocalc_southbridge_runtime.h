#pragma once

#include <cstdint>
#include <string_view>

#include <lexxos/devices/picocalc/picocalc_southbridge.h>

namespace lexxos::devices {

// Runtime contract for the provider-owned PicoCalc STM32 southbridge. The
// keyboard driver is the normal provider because it owns FIFO consumption;
// shell/apps use this interface for cached state and safe commands.
class PicoCalcSouthbridgeRuntimeService {
public:
	virtual ~PicoCalcSouthbridgeRuntimeService() = default;

	[[nodiscard]] virtual std::string_view name() const = 0;
	[[nodiscard]] virtual bool isReady() const = 0;

	[[nodiscard]] virtual bool southbridgeSnapshot(PicoCalcSouthbridgeSnapshot& snapshot) const = 0;
	[[nodiscard]] virtual std::uint32_t southbridgeSnapshotMs() const = 0;
	[[nodiscard]] virtual std::uint32_t southbridgeFullSnapshotMs() const = 0;
	[[nodiscard]] virtual bool southbridgeLastRefreshOk() const = 0;
	[[nodiscard]] virtual std::uint32_t southbridgeSnapshotUpdates() const = 0;
	[[nodiscard]] virtual std::uint32_t southbridgeSnapshotFailures() const = 0;
	[[nodiscard]] virtual PicoCalcSouthbridgeCommandStatus southbridgeLastCommandStatus() const = 0;
	[[nodiscard]] virtual bool refreshSouthbridgeSnapshot(std::uint32_t nowMs) = 0;

	[[nodiscard]] virtual bool requestLcdBacklight(std::uint8_t value, std::uint32_t nowMs) = 0;
	[[nodiscard]] virtual bool requestKeyboardBacklight(std::uint8_t value, std::uint32_t nowMs) = 0;
	[[nodiscard]] virtual bool requestSouthbridgeReset(std::uint8_t delaySeconds, std::uint32_t nowMs) = 0;
	[[nodiscard]] virtual bool requestPowerOff(std::uint32_t nowMs) = 0;

#if defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
	[[nodiscard]] virtual bool readJcsFirmwareInfo(PicoCalcJcsFirmwareInfo& info) const = 0;
	[[nodiscard]] virtual bool readJcsRtcConfig(PicoCalcJcsRtcConfig& config) const = 0;
	[[nodiscard]] virtual bool writeJcsRtcConfig(PicoCalcJcsRtcConfig config, std::uint32_t nowMs) = 0;
	[[nodiscard]] virtual bool readJcsRtcDateTime(lexxos::devices::rtc::DateTime& value) const = 0;
	[[nodiscard]] virtual bool writeJcsRtcDateTime(lexxos::devices::rtc::DateTime const& value, std::uint32_t nowMs) = 0;
	[[nodiscard]] virtual bool readJcsRtcAlarm(lexxos::devices::rtc::DateTime& value) const = 0;
	[[nodiscard]] virtual bool writeJcsRtcAlarm(lexxos::devices::rtc::DateTime const& value, std::uint32_t nowMs) = 0;
#endif

	[[nodiscard]] virtual std::uint32_t rawState() const = 0;
	[[nodiscard]] virtual std::uint32_t rawKeyCount() const = 0;
	[[nodiscard]] virtual std::uint32_t staleScanCodeReleases() const = 0;
	[[nodiscard]] virtual std::uint32_t staleReleaseCount() const = 0;
};

} // namespace lexxos::devices
