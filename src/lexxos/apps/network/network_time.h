#pragma once

#include <cstdint>
#include <string_view>

#include <lexxos/devices/rtc/date_time.h>

namespace lexxos::os {
class AppContext;
} // namespace lexxos::os

namespace lexxos::apps::network_programs {

[[nodiscard]] bool queryNetworkTime(lexxos::os::AppContext& context,
    std::string_view server,
    std::uint32_t timeoutMs,
    lexxos::devices::rtc::DateTime& output,
    std::uint32_t& unixSeconds);

} // namespace lexxos::apps::network_programs
