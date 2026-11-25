#pragma once

#include <cstdint>

#include <lexxos/apps/rtc/detail/rtc_backend_types.h>

namespace lexxos::apps::rtc_programs::detail {

[[nodiscard]] bool readDateTime(RtcBackend& backend, lexxos::devices::rtc::DateTime& output);
[[nodiscard]] bool writeDateTime(
    RtcBackend& backend,
    lexxos::devices::rtc::DateTime const& value,
    std::uint32_t nowMs,
    lexxos::devices::rtc::software::SetSource softwareSource = lexxos::devices::rtc::software::SetSource::Manual);
[[nodiscard]] bool readStatus(RtcBackend& backend, RtcBackendStatus& status);

} // namespace lexxos::apps::rtc_programs::detail
