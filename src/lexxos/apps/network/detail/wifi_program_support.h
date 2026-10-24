#pragma once

#include <cstdint>

#include <lexxos/os/app_context.h>
#include <lexxos/platform/board.h>
#include <lexxos/program/arguments.h>

namespace lexxos::apps::network_programs::detail {

struct WifiRuntimeStatus {
	bool supported{false};
	bool initialized{false};
	bool staModeEnabled{false};
	bool started{false};
	bool connected{false};
	bool gotIp{false};
	bool rssiValid{false};
	std::int32_t rssi{0};
	std::uint8_t signalLevel{0};
};

[[nodiscard]] WifiRuntimeStatus readWifiRuntimeStatus(lexxos::platform::BoardDescriptor const& board);

[[nodiscard]] int runWifiConnect(
    lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments,
    lexxos::platform::BoardDescriptor const& board);
[[nodiscard]] int runWifiStatus(
    lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments,
    lexxos::platform::BoardDescriptor const& board);
[[nodiscard]] int runWifiScan(
    lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments,
    lexxos::platform::BoardDescriptor const& board);

} // namespace lexxos::apps::network_programs::detail
