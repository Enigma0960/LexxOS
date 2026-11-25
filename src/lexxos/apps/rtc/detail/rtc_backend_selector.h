#pragma once

#include <string_view>

namespace lexxos::os {

class AppContext;

} // namespace lexxos::os

namespace lexxos::program {

class ParsedArguments;

} // namespace lexxos::program

namespace lexxos::apps::rtc_programs::detail {

struct RtcBackend;

[[nodiscard]] bool openRtcBackend(lexxos::os::AppContext& context,
    std::string_view programName,
    lexxos::program::ParsedArguments const& arguments,
    RtcBackend& backend);

} // namespace lexxos::apps::rtc_programs::detail
