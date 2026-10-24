#pragma once

#include <cstdint>
#include <string_view>

#include <lexxos/devices/rtc/date_time.h>
#include <lexxos/os/app_context.h>
#include <lexxos/platform/board.h>
#include <lexxos/program/arguments.h>

namespace lexxos::apps::network_programs::pico_detail {

struct WifiRuntimeStatus {
	bool supported{false};
	bool initialized{false};
	bool staModeEnabled{false};
	bool connected{false};
	bool rssiValid{false};
	std::int32_t rssi{0};
	std::uint8_t signalLevel{0};
};

[[nodiscard]] WifiRuntimeStatus readWifiRuntimeStatusOnPico();

[[nodiscard]] int runWifiConnectOnPico(
    lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments,
    lexxos::platform::BoardDescriptor const& board);
[[nodiscard]] bool queryNetworkTimeOnPico(
    lexxos::os::AppContext& context,
    std::string_view server,
    std::uint32_t timeoutMs,
    lexxos::devices::rtc::DateTime& output,
    std::uint32_t& unixSeconds);
[[nodiscard]] int runWifiStatusOnPico(
    lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments,
    lexxos::platform::BoardDescriptor const& board);
[[nodiscard]] int runPingOnPico(
    lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments,
    lexxos::platform::BoardDescriptor const& board);
[[nodiscard]] int runWifiScanOnPico(
    lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments,
    lexxos::platform::BoardDescriptor const& board);

} // namespace lexxos::apps::network_programs::pico_detail
