#include <lexxos/apps/rtc/detail/rtc_backend.h>

namespace lexxos::apps::rtc_programs::detail {

using lexxos::devices::rtc::DateTime;

bool readDateTime(RtcBackend& backend, DateTime& output) {
	switch (backend.kind) {
		case RtcBackendKind::Software:
			return backend.platform != nullptr && lexxos::devices::rtc::software::read(*backend.platform, output);
#if FEATURE_ENABLED(DS3231_RTC)
		case RtcBackendKind::Ds3231:
			return backend.ds3231 != nullptr && backend.ds3231->readDateTime(output);
#endif
		case RtcBackendKind::Pcf8563:
			return backend.pcf8563 != nullptr && backend.pcf8563->readDateTime(output);
#if FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
		case RtcBackendKind::PicoCalcSouthbridge:
			return backend.picoCalcSouthbridge != nullptr && backend.picoCalcSouthbridge->readJcsRtcDateTime(output);
#endif
		case RtcBackendKind::None:
			break;
		default:
			break;
	}

	return false;
}

bool writeDateTime(
    RtcBackend& backend,
    DateTime const& value,
    std::uint32_t nowMs,
    lexxos::devices::rtc::software::SetSource softwareSource) {
	switch (backend.kind) {
		case RtcBackendKind::Software:
			(void) nowMs;
			return backend.platform != nullptr
			    && lexxos::devices::rtc::software::set(*backend.platform,
			        value,
			        softwareSource);
#if FEATURE_ENABLED(DS3231_RTC)
		case RtcBackendKind::Ds3231:
			return backend.ds3231 != nullptr && backend.ds3231->writeDateTime(value);
#endif
		case RtcBackendKind::Pcf8563:
			return backend.pcf8563 != nullptr && backend.pcf8563->writeDateTime(value);
#if FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
		case RtcBackendKind::PicoCalcSouthbridge:
			return backend.picoCalcSouthbridge != nullptr && backend.picoCalcSouthbridge->writeJcsRtcDateTime(value, nowMs);
#endif
		case RtcBackendKind::None:
			break;
		default:
			break;
	}

	return false;
}

bool readStatus(RtcBackend& backend, RtcBackendStatus& status) {
	status = {};
	switch (backend.kind) {
		case RtcBackendKind::Software:
			if (backend.platform == nullptr) {
				return false;
			}

			status.softwareStatus = lexxos::devices::rtc::software::status(*backend.platform);
			status.softwareStatusValid = status.softwareStatus.configured;
			return status.softwareStatusValid;
#if FEATURE_ENABLED(DS3231_RTC)
		case RtcBackendKind::Ds3231: {
			if (backend.ds3231 == nullptr) {
				return false;
			}

			lexxos::devices::rtc::Ds3231Status dsStatus{};
			if (!backend.ds3231->readStatus(dsStatus)) {
				return false;
			}

			status.rawValid = true;
			status.raw = dsStatus.raw;
			status.oscillatorStoppedValid = true;
			status.oscillatorStopped = dsStatus.oscillatorStopped;
			return true;
		}
#endif
		case RtcBackendKind::Pcf8563: {
			if (backend.pcf8563 == nullptr) {
				return false;
			}

			lexxos::devices::rtc::Pcf8563Status pcfStatus{};
			if (!backend.pcf8563->readStatus(pcfStatus)) {
				return false;
			}

			status.rawValid = true;
			status.raw = pcfStatus.secondsRaw;
			status.voltageLowValid = true;
			status.voltageLow = pcfStatus.voltageLow;
			return true;
		}
#if FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
		case RtcBackendKind::PicoCalcSouthbridge: {
			if (backend.picoCalcSouthbridge == nullptr) {
				return false;
			}

			bool ok = false;
			lexxos::devices::PicoCalcJcsFirmwareInfo firmware{};
			if (backend.picoCalcSouthbridge->readJcsFirmwareInfo(firmware)) {
				status.jcsFirmware = firmware;
				status.jcsFirmwareValid = true;
				ok = true;
			}

			lexxos::devices::PicoCalcJcsRtcConfig config{};
			if (backend.picoCalcSouthbridge->readJcsRtcConfig(config)) {
				status.jcsRtcConfig = config;
				status.jcsRtcConfigValid = true;
				status.raw = config.raw;
				status.rawValid = true;
				ok = true;
			}

			return ok;
		}
#endif
		case RtcBackendKind::None:
			break;
		default:
			break;
	}

	return false;
}

} // namespace lexxos::apps::rtc_programs::detail
