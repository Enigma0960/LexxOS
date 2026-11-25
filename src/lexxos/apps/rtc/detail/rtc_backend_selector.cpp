#include <lexxos/apps/rtc/detail/rtc_backend_selector.h>

#include <new>
#include <utility>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/rtc/detail/rtc_program_support.h>
#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/picocalc/picocalc_southbridge_resource_policy.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::rtc_programs::detail {

namespace {

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

} // namespace lexxos::apps::rtc_programs::detail
