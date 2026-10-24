#include <lexxos/apps/rtc/detail/rtc_program_support.h>

#include <new>
#include <utility>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/picocalc/picocalc_southbridge_resource_policy.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::rtc_programs::detail {

namespace {

	using lexxos::devices::rtc::DateTime;

#if FEATURE_ENABLED(DS3231_RTC)
	using lexxos::devices::rtc::Ds3231Rtc;
#endif
	using lexxos::devices::rtc::Pcf8563Rtc;

	enum class RequestedRtcSource : std::uint8_t {
		Auto,
		Software,
		Ds3231,
		Pcf8563,
		PicoCalcSouthbridge,
	};

	char hexDigit(std::uint8_t value) {
		return static_cast<char>(value < 10U ? '0' + value : 'A' + value - 10U);
	}

	void writePadded2(lexxos::os::AppContext& context, std::uint8_t value) {
		char buffer[3]{
		    static_cast<char>('0' + (value / 10U)),
		    static_cast<char>('0' + (value % 10U)),
		    '\0',
		};
		context.write(buffer);
	}

	void writePadded4(lexxos::os::AppContext& context, std::uint16_t value) {
		char buffer[5]{
		    static_cast<char>('0' + ((value / 1000U) % 10U)),
		    static_cast<char>('0' + ((value / 100U) % 10U)),
		    static_cast<char>('0' + ((value / 10U) % 10U)),
		    static_cast<char>('0' + (value % 10U)),
		    '\0',
		};
		context.write(buffer);
	}

	void formatHex2(std::uint8_t value, char (&buffer)[5]) {
		buffer[0] = '0';
		buffer[1] = 'x';
		buffer[2] = hexDigit(static_cast<std::uint8_t>((value >> 4U) & 0x0FU));
		buffer[3] = hexDigit(static_cast<std::uint8_t>(value & 0x0FU));
		buffer[4] = '\0';
	}

	bool parseFixedUnsigned(std::string_view text, std::size_t offset, std::size_t width, std::uint16_t& output) {
		if (offset + width > text.size() || width == 0U) {
			return false;
		}

		std::uint16_t value = 0;
		for (std::size_t index = 0; index < width; ++index) {
			char const character = text[offset + index];
			if (character < '0' || character > '9') {
				return false;
			}
			value = static_cast<std::uint16_t>(value * 10U + static_cast<std::uint16_t>(character - '0'));
		}

		output = value;
		return true;
	}

	bool parseDate(std::string_view text, DateTime& output) {
		if (text.size() != 10U || text[4] != '-' || text[7] != '-') {
			return false;
		}

		std::uint16_t year = 0;
		std::uint16_t month = 0;
		std::uint16_t day = 0;
		if (!parseFixedUnsigned(text, 0, 4, year)
		    || !parseFixedUnsigned(text, 5, 2, month)
		    || !parseFixedUnsigned(text, 8, 2, day)) {
			return false;
		}

		output.year = year;
		output.month = static_cast<std::uint8_t>(month);
		output.day = static_cast<std::uint8_t>(day);
		return true;
	}

	bool parseTime(std::string_view text, DateTime& output) {
		if (text.size() != 8U || text[2] != ':' || text[5] != ':') {
			return false;
		}

		std::uint16_t hour = 0;
		std::uint16_t minute = 0;
		std::uint16_t second = 0;
		if (!parseFixedUnsigned(text, 0, 2, hour)
		    || !parseFixedUnsigned(text, 3, 2, minute)
		    || !parseFixedUnsigned(text, 6, 2, second)) {
			return false;
		}

		output.hour = static_cast<std::uint8_t>(hour);
		output.minute = static_cast<std::uint8_t>(minute);
		output.second = static_cast<std::uint8_t>(second);
		return true;
	}

	bool parseI2cAddress(std::string_view text, std::uint8_t& output) {
		if (text.empty()) {
			return false;
		}

		std::uint32_t value = 0;
		if (text.size() > 2U && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
			if (text.size() == 2U) {
				return false;
			}
			for (std::size_t index = 2; index < text.size(); ++index) {
				char const character = text[index];
				std::uint32_t digit = 0;
				if (character >= '0' && character <= '9') {
					digit = static_cast<std::uint32_t>(character - '0');
				} else if (character >= 'a' && character <= 'f') {
					digit = static_cast<std::uint32_t>(10 + character - 'a');
				} else if (character >= 'A' && character <= 'F') {
					digit = static_cast<std::uint32_t>(10 + character - 'A');
				} else {
					return false;
				}
				value = value * 16U + digit;
			}
		} else if (!parseUnsigned32(text, value)) {
			return false;
		}

		if (value > 0x7FU) {
			return false;
		}
		output = static_cast<std::uint8_t>(value);
		return true;
	}

	bool parseSource(std::string_view text, RequestedRtcSource& output) {
		if (text.empty() || text == "auto") {
			output = RequestedRtcSource::Auto;
			return true;
		}
		if (text == "software" || text == "soft") {
			output = RequestedRtcSource::Software;
			return true;
		}
		if (text == "ds3231") {
			output = RequestedRtcSource::Ds3231;
			return true;
		}
		if (text == "pcf8563" || text == "bm8563") {
			output = RequestedRtcSource::Pcf8563;
			return true;
		}
		if (text == "picocalc") {
			output = RequestedRtcSource::PicoCalcSouthbridge;
			return true;
		}
		return false;
	}

	bool openSoftwareRtc(lexxos::os::AppContext& context,
	    std::string_view programName,
	    RtcBackend& backend,
	    bool explicitSource,
	    bool& fatal) {
		fatal = false;
		if (!lexxos::devices::rtc::software::configured(context.platform())) {
			if (explicitSource) {
				context.writeError(programName);
				context.writeErrorLine(": software RTC backend is not enabled by this board profile");
				fatal = true;
			}
			return false;
		}

		backend.kind = RtcBackendKind::Software;
		return true;
	}

#if FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
	bool tryOpenPicoCalcRtc(lexxos::os::AppContext& context,
	    std::string_view programName,
	    RtcBackend& backend,
	    bool explicitSource,
	    bool& fatal) {
		fatal = false;
		auto* service = lexxos::devices::PicoCalcSouthbridgeResourcePolicy::readyService(context.devices());
		if (service == nullptr) {
			if (explicitSource) {
				context.writeError(programName);
				context.writeErrorLine(": PicoCalc southbridge RTC backend is unavailable");
				fatal = true;
			}
			return false;
		}

		auto lock = lexxos::devices::PicoCalcSouthbridgeResourcePolicy::lock(
		    context.devices(),
		    programName,
		    lexxos::devices::DeviceResourcePriority::Interactive);
		if (!lock) {
			context.writeError(programName);
			context.writeErrorLine(": PicoCalc southbridge service is busy");
			fatal = true;
			return false;
		}

		backend.kind = RtcBackendKind::PicoCalcSouthbridge;
		backend.lock = std::move(lock);
		backend.picoCalcSouthbridge = service;
		return true;
	}
#endif

#if FEATURE_ENABLED(DS3231_RTC)
	bool openDs3231Rtc(lexxos::os::AppContext& context,
	    std::string_view programName,
	    lexxos::program::ParsedArguments const& arguments,
	    RtcBackend& backend,
	    bool explicitSource,
	    bool& fatal) {
		fatal = false;
		auto const busName = arguments.valueOr("bus", "debug");
		std::uint8_t address = Ds3231Rtc::defaultAddress;
		if (arguments.has("address") && !parseI2cAddress(arguments.value("address"), address)) {
			if (explicitSource) {
				context.writeError(programName);
				context.writeErrorLine(": invalid --address value");
			}
			fatal = explicitSource;
			return false;
		}

		auto* bus = context.devices().i2cBus(busName);
		if (bus == nullptr) {
			if (explicitSource) {
				context.writeError(programName);
				context.writeError(": unknown I2C bus: ");
				context.writeErrorLine(busName);
			}
			fatal = explicitSource;
			return false;
		}

		auto lock = context.devices().lockI2cBus(busName, lexxos::devices::DeviceResourcePriority::Interactive, programName);
		if (!lock) {
			context.writeError(programName);
			context.writeError(": I2C bus is busy: ");
			context.writeErrorLine(busName);
			fatal = true;
			return false;
		}

		if (!bus->isReady()) {
			if (explicitSource) {
				context.writeError(programName);
				context.writeError(": I2C bus is not ready: ");
				context.writeErrorLine(busName);
			}
			fatal = explicitSource;
			return false;
		}

		auto* rtc = new (backend.ds3231Storage.data()) Ds3231Rtc{*bus, address};
		if (!rtc->probe()) {
			if (explicitSource) {
				context.writeError(programName);
				context.writeError(": DS3231 is not detected at ");
				writeErrorHex2(context, address);
				context.writeErrorLine("");
			}
			fatal = false;
			return false;
		}

		backend.kind = RtcBackendKind::Ds3231;
		backend.lock = std::move(lock);
		backend.ds3231 = rtc;
		return true;
	}
#endif

	bool openPcf8563Rtc(lexxos::os::AppContext& context,
	    std::string_view programName,
	    lexxos::program::ParsedArguments const& arguments,
	    RtcBackend& backend,
	    bool explicitSource,
	    bool& fatal) {
		fatal = false;
		auto const busName = arguments.valueOr("bus", "debug");
		std::uint8_t address = Pcf8563Rtc::defaultAddress;
		if (arguments.has("address") && !parseI2cAddress(arguments.value("address"), address)) {
			if (explicitSource) {
				context.writeError(programName);
				context.writeErrorLine(": invalid --address value");
			}
			fatal = explicitSource;
			return false;
		}

		auto* bus = context.devices().i2cBus(busName);
		if (bus == nullptr) {
			if (explicitSource) {
				context.writeError(programName);
				context.writeError(": unknown I2C bus: ");
				context.writeErrorLine(busName);
			}
			fatal = explicitSource;
			return false;
		}

		auto lock = context.devices().lockI2cBus(busName, lexxos::devices::DeviceResourcePriority::Interactive, programName);
		if (!lock) {
			context.writeError(programName);
			context.writeError(": I2C bus is busy: ");
			context.writeErrorLine(busName);
			fatal = true;
			return false;
		}

		if (!bus->isReady()) {
			if (explicitSource) {
				context.writeError(programName);
				context.writeError(": I2C bus is not ready: ");
				context.writeErrorLine(busName);
			}
			fatal = explicitSource;
			return false;
		}

		auto* rtc = new (backend.pcf8563Storage.data()) Pcf8563Rtc{*bus, address};
		if (!rtc->probe()) {
			if (explicitSource) {
				context.writeError(programName);
				context.writeError(": PCF8563/BM8563 is not detected at ");
				writeErrorHex2(context, address);
				context.writeErrorLine("");
			}
			fatal = false;
			return false;
		}

		backend.kind = RtcBackendKind::Pcf8563;
		backend.lock = std::move(lock);
		backend.pcf8563 = rtc;
		return true;
	}

} // namespace

void writeDateTime(lexxos::os::AppContext& context, DateTime const& value) {
	writePadded4(context, value.year);
	context.write("-");
	writePadded2(context, value.month);
	context.write("-");
	writePadded2(context, value.day);
	context.write(" ");
	writePadded2(context, value.hour);
	context.write(":");
	writePadded2(context, value.minute);
	context.write(":");
	writePadded2(context, value.second);
}

void writeHex2(lexxos::os::AppContext& context, std::uint8_t value) {
	char buffer[5]{};
	formatHex2(value, buffer);
	context.write(buffer);
}

void writeErrorHex2(lexxos::os::AppContext& context, std::uint8_t value) {
	char buffer[5]{};
	formatHex2(value, buffer);
	context.writeError(buffer);
}

std::string_view backendName(RtcBackendKind kind) {
	switch (kind) {
		case RtcBackendKind::Software:
			return "software";
		case RtcBackendKind::Ds3231:
			return "ds3231";
		case RtcBackendKind::Pcf8563:
			return "pcf8563";
		case RtcBackendKind::PicoCalcSouthbridge:
			return "picocalc-southbridge";
		case RtcBackendKind::None:
			return "none";
	}
	return "none";
}

bool parseDateTime(std::string_view dateText, std::string_view timeText, DateTime& output) {
	DateTime value{};
	auto const separator = dateText.find('T');
	if (separator != std::string_view::npos) {
		if (!timeText.empty() || !parseDate(dateText.substr(0, separator), value) || !parseTime(dateText.substr(separator + 1U), value)) {
			return false;
		}
	} else {
		if (!parseDate(dateText, value) || timeText.empty() || !parseTime(timeText, value)) {
			return false;
		}
	}

	value.weekday = lexxos::devices::rtc::weekdayIso(value);
	if (!lexxos::devices::rtc::isValidDate(value)) {
		return false;
	}

	output = value;
	return true;
}

bool openRtcBackend(lexxos::os::AppContext& context,
    std::string_view programName,
    lexxos::program::ParsedArguments const& arguments,
    RtcBackend& backend) {
	backend.kind = RtcBackendKind::None;
	backend.platform = &context.platform();

	RequestedRtcSource source = RequestedRtcSource::Auto;
	if (!parseSource(arguments.valueOr("source", "auto"), source)) {
		context.writeError(programName);
		context.writeErrorLine(": invalid --source value, expected auto, software, picocalc, ds3231, pcf8563 or bm8563");
		return false;
	}

	if (source == RequestedRtcSource::Software
	    || (source == RequestedRtcSource::Auto && lexxos::devices::rtc::software::preferredAsDefault(context.platform()))) {
		bool fatal = false;
		if (openSoftwareRtc(context, programName, backend, source == RequestedRtcSource::Software, fatal)) {
			return true;
		}
		if (fatal || source == RequestedRtcSource::Software) {
			return false;
		}
	}

	if (source == RequestedRtcSource::PicoCalcSouthbridge || source == RequestedRtcSource::Auto) {
#if FEATURE_ENABLED(PICO_CALC) && defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
		bool fatal = false;
		if (tryOpenPicoCalcRtc(context, programName, backend, source == RequestedRtcSource::PicoCalcSouthbridge, fatal)) {
			return true;
		}
		if (fatal) {
			return false;
		}
#else
		if (source == RequestedRtcSource::PicoCalcSouthbridge) {
			context.writeError(programName);
			context.writeErrorLine(": PicoCalc southbridge RTC backend is unavailable in this build");
			return false;
		}
#endif
	}

	if (source == RequestedRtcSource::Ds3231 || source == RequestedRtcSource::Auto) {
#if FEATURE_ENABLED(DS3231_RTC)
		bool fatal = false;
		if (openDs3231Rtc(context, programName, arguments, backend, source == RequestedRtcSource::Ds3231, fatal)) {
			return true;
		}
		if (fatal || source == RequestedRtcSource::Ds3231) {
			return false;
		}
#else
		if (source == RequestedRtcSource::Ds3231) {
			context.writeError(programName);
			context.writeErrorLine(": DS3231 RTC backend is disabled in this build");
			return false;
		}
#endif
	}

	if (source == RequestedRtcSource::Pcf8563 || source == RequestedRtcSource::Auto) {
		bool fatal = false;
		if (openPcf8563Rtc(context, programName, arguments, backend, source == RequestedRtcSource::Pcf8563, fatal)) {
			return true;
		}
		if (fatal || source == RequestedRtcSource::Pcf8563) {
			return false;
		}
	}

	if (source == RequestedRtcSource::Auto && !lexxos::devices::rtc::software::preferredAsDefault(context.platform())) {
		bool fatal = false;
		if (openSoftwareRtc(context, programName, backend, false, fatal)) {
			return true;
		}
		if (fatal) {
			return false;
		}
	}

	context.writeError(programName);
	context.writeErrorLine(": no RTC backend is available");
	return false;
}

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
