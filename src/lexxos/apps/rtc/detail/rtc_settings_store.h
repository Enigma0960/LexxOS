#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include <lexxos/platform/board.h>

namespace lexxos::apps::rtc_programs::detail {

inline constexpr std::string_view rtcSettingsPath = "/mnt/sd/.lexx/rtc.conf";
inline constexpr std::size_t rtcSettingsServerMaxLength = 127;

struct RtcSettings {
	bool enabled{true};
	bool preferAsDefault{true};
	bool autoNetworkSync{true};
	std::array<char, rtcSettingsServerMaxLength + 1U> server{};
	std::uint32_t timeoutMs{8000};
	std::uint32_t intervalMs{60U * 60U * 1000U};
	std::uint32_t retryMs{60U * 1000U};
	std::uint32_t startupDelayMs{3000};
	std::int32_t timezoneOffsetSeconds{0};
	std::array<char, 16> source{};
	std::array<char, 32> bus{};
	bool addressSet{false};
	std::uint8_t address{0};
};

enum class RtcSettingsStatus : std::uint8_t {
	Ok,
	NotFound,
	Invalid,
	IoError,
};

[[nodiscard]] std::string_view toString(RtcSettingsStatus status);
[[nodiscard]] std::string_view rtcSettingsServer(RtcSettings const& settings);
[[nodiscard]] std::string_view rtcSettingsSource(RtcSettings const& settings);
[[nodiscard]] std::string_view rtcSettingsBus(RtcSettings const& settings);
[[nodiscard]] bool setRtcSettingsServer(RtcSettings& settings, std::string_view server);
[[nodiscard]] bool setRtcSettingsSource(RtcSettings& settings, std::string_view source);
[[nodiscard]] bool setRtcSettingsBus(RtcSettings& settings, std::string_view bus);
[[nodiscard]] bool parseRtcTimezoneOffset(std::string_view text, std::int32_t& output);
void formatRtcTimezoneOffset(std::int32_t offsetSeconds, char (&output)[10]);
[[nodiscard]] bool parseRtcI2cAddress(std::string_view text, std::uint8_t& output);
[[nodiscard]] bool isSupportedRtcSource(std::string_view source);
void resetRtcSettingsDefaults(RtcSettings& output, lexxos::platform::SoftwareRtcConfig const& config);
[[nodiscard]] bool validateRtcSettings(RtcSettings const& settings, std::string_view& error);
[[nodiscard]] RtcSettingsStatus loadRtcSettings(
    RtcSettings& output,
    lexxos::platform::SoftwareRtcConfig const& defaults);
[[nodiscard]] RtcSettingsStatus saveRtcSettings(RtcSettings const& settings);
[[nodiscard]] RtcSettingsStatus clearRtcSettings();
[[nodiscard]] lexxos::platform::SoftwareRtcConfig softwareRtcConfigFromSettings(RtcSettings const& settings);

} // namespace lexxos::apps::rtc_programs::detail
