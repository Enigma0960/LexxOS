#pragma once

#include <cstdint>
#include <string_view>

#include <lexxos/devices/picocalc/picocalc_southbridge_runtime.h>

namespace lexxos::devices::input_detail {

class PicoCalcKeyboardController;

class PicoCalcKeyboardRuntimeService final : public PicoCalcSouthbridgeRuntimeService {
public:
	explicit PicoCalcKeyboardRuntimeService(PicoCalcKeyboardController& controller);

	[[nodiscard]] std::string_view name() const override;
	[[nodiscard]] bool isReady() const override;

	[[nodiscard]] bool southbridgeSnapshot(PicoCalcSouthbridgeSnapshot& snapshot) const override;
	[[nodiscard]] std::uint32_t southbridgeSnapshotMs() const override;
	[[nodiscard]] std::uint32_t southbridgeFullSnapshotMs() const override;
	[[nodiscard]] bool southbridgeLastRefreshOk() const override;
	[[nodiscard]] std::uint32_t southbridgeSnapshotUpdates() const override;
	[[nodiscard]] std::uint32_t southbridgeSnapshotFailures() const override;
	[[nodiscard]] PicoCalcSouthbridgeCommandStatus southbridgeLastCommandStatus() const override;
	[[nodiscard]] bool refreshSouthbridgeSnapshot(std::uint32_t nowMs) override;

	[[nodiscard]] bool requestLcdBacklight(std::uint8_t value, std::uint32_t nowMs) override;
	[[nodiscard]] bool requestKeyboardBacklight(std::uint8_t value, std::uint32_t nowMs) override;
	[[nodiscard]] bool requestSouthbridgeReset(std::uint8_t delaySeconds, std::uint32_t nowMs) override;
	[[nodiscard]] bool requestPowerOff(std::uint32_t nowMs) override;
#if defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
	[[nodiscard]] bool readJcsFirmwareInfo(PicoCalcJcsFirmwareInfo& info) const override;
	[[nodiscard]] bool readJcsRtcConfig(PicoCalcJcsRtcConfig& config) const override;
	[[nodiscard]] bool writeJcsRtcConfig(PicoCalcJcsRtcConfig config, std::uint32_t nowMs) override;
	[[nodiscard]] bool readJcsRtcDateTime(lexxos::devices::rtc::DateTime& value) const override;
	[[nodiscard]] bool writeJcsRtcDateTime(lexxos::devices::rtc::DateTime const& value, std::uint32_t nowMs) override;
	[[nodiscard]] bool readJcsRtcAlarm(lexxos::devices::rtc::DateTime& value) const override;
	[[nodiscard]] bool writeJcsRtcAlarm(lexxos::devices::rtc::DateTime const& value, std::uint32_t nowMs) override;
#endif
	[[nodiscard]] std::uint32_t rawState() const override;
	[[nodiscard]] std::uint32_t rawKeyCount() const override;
	[[nodiscard]] std::uint32_t staleScanCodeReleases() const override;
	[[nodiscard]] std::uint32_t staleReleaseCount() const override;

private:
	PicoCalcKeyboardController& _controller;
};

} // namespace lexxos::devices::input_detail
