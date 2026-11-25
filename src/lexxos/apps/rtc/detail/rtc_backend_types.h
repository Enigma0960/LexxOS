#pragma once

#include <array>
#include <cstdint>

#include <lexxos/devices/device_resource_lock.h>
#include <lexxos/devices/rtc/date_time.h>
#include <lexxos/devices/rtc/pcf8563.h>
#include <lexxos/devices/rtc/software_rtc.h>
#include <lexxos/feature.h>

#if FEATURE_ENABLED(DS3231_RTC)
	#include <lexxos/devices/rtc/ds3231.h>
#endif

#if FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
	#include <lexxos/devices/picocalc/picocalc_southbridge_runtime.h>
#endif

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

} // namespace lexxos::apps::rtc_programs::detail
