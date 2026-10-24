#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/rtc/date_time.h>
#include <lexxos/devices/rtc/pcf8563.h>
#include <lexxos/devices/rtc/software_rtc.h>
#include <lexxos/feature.h>
#include <lexxos/program/program.h>

#if FEATURE_ENABLED(DS3231_RTC)
	#include <lexxos/devices/rtc/ds3231.h>
#endif

#if FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
	#include <lexxos/devices/picocalc/picocalc_southbridge_runtime.h>
#endif

namespace lexxos::os {
class AppContext;
} // namespace lexxos::os

namespace lexxos::platform {
struct PlatformServices;
} // namespace lexxos::platform

namespace lexxos::apps::rtc_programs::detail {

#if FEATURE_ENABLED(DS3231_RTC)
using RtcStorage = std::array<std::uint8_t, sizeof(lexxos::devices::rtc::Ds3231Rtc)>;
#endif
using Pcf8563Storage = std::array<std::uint8_t, sizeof(lexxos::devices::rtc::Pcf8563Rtc)>;

enum class RtcBackendKind : std::uint8_t {
	None,
	Software,
	Ds3231,
	Pcf8563,
	PicoCalcSouthbridge,
};

struct RtcBackendStatus {
	bool rawValid{false};
	std::uint8_t raw{0};
	bool oscillatorStoppedValid{false};
	bool oscillatorStopped{false};
	bool voltageLowValid{false};
	bool voltageLow{false};
	bool softwareStatusValid{false};
	lexxos::devices::rtc::software::Status softwareStatus{};
#if FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
	bool jcsFirmwareValid{false};
	lexxos::devices::PicoCalcJcsFirmwareInfo jcsFirmware{};
	bool jcsRtcConfigValid{false};
	lexxos::devices::PicoCalcJcsRtcConfig jcsRtcConfig{};
#endif
};

struct RtcBackend {
	RtcBackendKind kind{RtcBackendKind::None};
	lexxos::platform::PlatformServices const* platform{nullptr};
	lexxos::devices::DeviceResourceLock lock{};
#if FEATURE_ENABLED(DS3231_RTC)
	alignas(lexxos::devices::rtc::Ds3231Rtc) RtcStorage ds3231Storage{};
	lexxos::devices::rtc::Ds3231Rtc* ds3231{nullptr};
#endif
	alignas(lexxos::devices::rtc::Pcf8563Rtc) Pcf8563Storage pcf8563Storage{};
	lexxos::devices::rtc::Pcf8563Rtc* pcf8563{nullptr};
#if FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
	lexxos::devices::PicoCalcSouthbridgeRuntimeService* picoCalcSouthbridge{nullptr};
#endif
};

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
[[nodiscard]] bool openRtcBackend(lexxos::os::AppContext& context,
    std::string_view programName,
    lexxos::program::ParsedArguments const& arguments,
    RtcBackend& backend);
[[nodiscard]] bool readDateTime(RtcBackend& backend, lexxos::devices::rtc::DateTime& output);
[[nodiscard]] bool writeDateTime(
    RtcBackend& backend,
    lexxos::devices::rtc::DateTime const& value,
    std::uint32_t nowMs,
    lexxos::devices::rtc::software::SetSource softwareSource = lexxos::devices::rtc::software::SetSource::Manual);
[[nodiscard]] bool readStatus(RtcBackend& backend, RtcBackendStatus& status);

} // namespace lexxos::apps::rtc_programs::detail
