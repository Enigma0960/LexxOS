#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/devices/rtc/date_time.h>
#include <lexxos/platform/board.h>

namespace lexxos::platform {
struct PlatformServices;
}

namespace lexxos::devices::rtc::software {

enum class SetSource : std::uint8_t {
	None,
	Manual,
	Network,
};

struct Status {
	bool configured{false};
	bool valid{false};
	bool autoNetworkSync{false};
	char const* networkSyncServer{nullptr};
	std::uint32_t networkSyncTimeoutMs{0};
	std::uint32_t networkSyncIntervalMs{0};
	std::uint32_t networkSyncRetryIntervalMs{0};
	std::uint32_t startupDelayMs{0};
	std::int32_t timezoneOffsetSeconds{0};
	DateTime now{};
	std::uint32_t unixSeconds{0};
	std::uint32_t baseUnixSeconds{0};
	std::uint32_t baseUptimeMs{0};
	std::uint32_t lastSetUptimeMs{0};
	std::uint32_t lastNetworkSyncUptimeMs{0};
	std::uint32_t lastNetworkAttemptUptimeMs{0};
	std::uint32_t setCount{0};
	std::uint32_t networkSyncCount{0};
	SetSource lastSetSource{SetSource::None};
	bool lastNetworkAttemptValid{false};
	bool lastNetworkAttemptOk{false};
};

[[nodiscard]] bool configured(lexxos::platform::BoardDescriptor const& board);
[[nodiscard]] bool configured(lexxos::platform::PlatformServices const& platform);
[[nodiscard]] bool preferredAsDefault(lexxos::platform::BoardDescriptor const& board);
[[nodiscard]] bool preferredAsDefault(lexxos::platform::PlatformServices const& platform);
[[nodiscard]] lexxos::platform::SoftwareRtcConfig config(lexxos::platform::PlatformServices const& platform);
[[nodiscard]] bool applyRuntimeConfigOverride(
    lexxos::platform::PlatformServices const& platform,
    lexxos::platform::SoftwareRtcConfig const& config,
    std::string_view networkSyncServer);
void clearRuntimeConfigOverride();
[[nodiscard]] bool runtimeConfigOverrideActive();
[[nodiscard]] bool read(lexxos::platform::PlatformServices const& platform, DateTime& output);
[[nodiscard]] bool readUnixSeconds(lexxos::platform::PlatformServices const& platform, std::uint32_t& output);
[[nodiscard]] bool set(lexxos::platform::PlatformServices const& platform, DateTime const& value, SetSource source);
[[nodiscard]] bool setUnixSeconds(lexxos::platform::PlatformServices const& platform, std::uint32_t unixSeconds, SetSource source);
void recordNetworkAttempt(lexxos::platform::PlatformServices const& platform, bool ok);
[[nodiscard]] Status status(lexxos::platform::PlatformServices const& platform);
[[nodiscard]] char const* toString(SetSource source);

} // namespace lexxos::devices::rtc::software
