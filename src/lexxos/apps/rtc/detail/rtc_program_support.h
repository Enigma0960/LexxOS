#pragma once

#include <cstdint>
#include <string_view>

#include <lexxos/apps/rtc/detail/rtc_backend.h>
#include <lexxos/apps/rtc/detail/rtc_backend_selector.h>
#include <lexxos/program/program.h>

namespace lexxos::apps::rtc_programs::detail {

inline constexpr lexxos::program::ProgramArgumentSpec rtcCommonArguments[] = {
    {
     .name = "source",
     .shortName = 's',
     .valueName = "auto|software|picocalc|ds3231|pcf8563",
     .help = "RTC backend source",
     .kind = lexxos::program::ProgramArgumentKind::Option,
     .valueType = lexxos::program::ProgramArgumentValueType::String,
     .required = false,
     .defaultValue = "auto",
     },
    {
     .name = "bus",
     .shortName = 'b',
     .valueName = "name",
     .help = "I2C bus name",
     .kind = lexxos::program::ProgramArgumentKind::Option,
     .valueType = lexxos::program::ProgramArgumentValueType::String,
     .required = false,
     .defaultValue = "debug",
     },
    {
     .name = "address",
     .shortName = 'a',
     .valueName = "addr",
     .help = "RTC 7-bit I2C address, decimal or hex; backend default when omitted",
     .kind = lexxos::program::ProgramArgumentKind::Option,
     .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
     .required = false,
     },
};

inline constexpr lexxos::program::ProgramArgumentSpec rtcSetArguments[] = {
    {
     .name = "source",
     .shortName = 's',
     .valueName = "auto|software|picocalc|ds3231|pcf8563",
     .help = "RTC backend source",
     .kind = lexxos::program::ProgramArgumentKind::Option,
     .valueType = lexxos::program::ProgramArgumentValueType::String,
     .required = false,
     .defaultValue = "auto",
     },
    {
     .name = "bus",
     .shortName = 'b',
     .valueName = "name",
     .help = "I2C bus name",
     .kind = lexxos::program::ProgramArgumentKind::Option,
     .valueType = lexxos::program::ProgramArgumentValueType::String,
     .required = false,
     .defaultValue = "debug",
     },
    {
     .name = "address",
     .shortName = 'a',
     .valueName = "addr",
     .help = "RTC 7-bit I2C address, decimal or hex; backend default when omitted",
     .kind = lexxos::program::ProgramArgumentKind::Option,
     .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
     .required = false,
     },
    {
     .name = "date",
     .valueName = "YYYY-MM-DD|YYYY-MM-DDTHH:MM:SS",
     .help = "date or ISO-like date/time",
     .kind = lexxos::program::ProgramArgumentKind::Positional,
     .valueType = lexxos::program::ProgramArgumentValueType::String,
     .required = true,
     },
    {
     .name = "time",
     .valueName = "HH:MM:SS",
     .help = "time when it is not included in the first argument",
     .kind = lexxos::program::ProgramArgumentKind::Positional,
     .valueType = lexxos::program::ProgramArgumentValueType::String,
     .required = false,
     },
};

void writeDateTime(lexxos::os::AppContext& context, lexxos::devices::rtc::DateTime const& value);
void writeHex2(lexxos::os::AppContext& context, std::uint8_t value);
void writeErrorHex2(lexxos::os::AppContext& context, std::uint8_t value);
[[nodiscard]] std::string_view backendName(RtcBackendKind kind);
[[nodiscard]] bool parseDateTime(std::string_view dateText,
    std::string_view timeText,
    lexxos::devices::rtc::DateTime& output);

} // namespace lexxos::apps::rtc_programs::detail
