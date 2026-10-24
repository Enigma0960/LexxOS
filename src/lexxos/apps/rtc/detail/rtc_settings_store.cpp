#include <lexxos/apps/rtc/detail/rtc_settings_store.h>

#include <array>
#include <cstddef>
#include <cstring>

#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::rtc_programs::detail {
namespace {

	constexpr std::string_view rtcSettingsDirectory = "/mnt/sd/.lexx";
	constexpr std::uint32_t rtcSettingsMaxTimeoutMs = 60000;
	constexpr std::size_t rtcSettingsLineBufferBytes = 192;
	constexpr std::uint32_t rtcSettingsMaxFileBytes = 2048;

	std::size_t cStringLength(char const* text, std::size_t capacity) {
		if (text == nullptr) {
			return 0;
		}
		std::size_t length = 0;
		while (length < capacity && text[length] != '\0') {
			++length;
		}
		return length;
	}

	template <std::size_t Capacity>
	std::string_view arrayString(std::array<char, Capacity> const& value) {
		return {value.data(), cStringLength(value.data(), value.size())};
	}

	template <std::size_t Capacity>
	bool copyString(std::string_view text, std::array<char, Capacity>& output) {
		if (text.size() >= output.size()) {
			return false;
		}
		for (std::size_t index = 0; index < text.size(); ++index) {
			char const character = text[index];
			if (character == '\r' || character == '\n') {
				return false;
			}
			output[index] = character;
		}
		output[text.size()] = '\0';
		for (std::size_t index = text.size() + 1U; index < output.size(); ++index) {
			output[index] = '\0';
		}
		return true;
	}

	bool parseUnsigned(std::string_view text, std::uint32_t& output) {
		if (text.empty()) {
			return false;
		}
		std::uint32_t value = 0;
		for (char const character : text) {
			if (character < '0' || character > '9') {
				return false;
			}
			std::uint32_t const digit = static_cast<std::uint32_t>(character - '0');
			if (value > (0xFFFFFFFFU - digit) / 10U) {
				return false;
			}
			value = value * 10U + digit;
		}
		output = value;
		return true;
	}

	bool parseSmallUnsigned(std::string_view text, std::uint32_t& output) {
		return parseUnsigned(text, output);
	}

	bool parseBoolean(std::string_view text, bool& output) {
		if (text == "1" || text == "true" || text == "yes" || text == "on") {
			output = true;
			return true;
		}
		if (text == "0" || text == "false" || text == "no" || text == "off") {
			output = false;
			return true;
		}
		return false;
	}

	std::string_view trimRightCarriageReturn(std::string_view text) {
		if (!text.empty() && text[text.size() - 1U] == '\r') {
			return text.substr(0, text.size() - 1U);
		}
		return text;
	}

	bool startsWith(std::string_view text, std::string_view prefix) {
		return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
	}

	bool appendText(lexxos::vfs::FileHandle& file, std::string_view text) {
		return file.write(text) == lexxos::vfs::WriteStatus::Ok;
	}

	bool appendUnsigned(lexxos::vfs::FileHandle& file, std::uint32_t value) {
		char text[11]{};
		std::size_t index = sizeof(text) - 1U;
		text[index] = '\0';
		do {
			text[--index] = static_cast<char>('0' + (value % 10U));
			value /= 10U;
		} while (value != 0 && index > 0);
		return appendText(file, {text + index, sizeof(text) - 1U - index});
	}

	bool appendLine(lexxos::vfs::FileHandle& file, std::string_view key, std::string_view value) {
		return appendText(file, key)
		    && appendText(file, "=")
		    && appendText(file, value)
		    && appendText(file, "\n");
	}

	bool appendLine(lexxos::vfs::FileHandle& file, std::string_view key, std::uint32_t value) {
		return appendText(file, key)
		    && appendText(file, "=")
		    && appendUnsigned(file, value)
		    && appendText(file, "\n");
	}

	bool appendLine(lexxos::vfs::FileHandle& file, std::string_view key, bool value) {
		return appendLine(file, key, value ? std::string_view{"1"} : std::string_view{"0"});
	}

	bool appendAddress(lexxos::vfs::FileHandle& file, std::uint8_t value) {
		char text[5]{};
		constexpr char digits[] = "0123456789ABCDEF";
		text[0] = '0';
		text[1] = 'x';
		text[2] = digits[(value >> 4U) & 0x0FU];
		text[3] = digits[value & 0x0FU];
		text[4] = '\0';
		return appendText(file, text);
	}

	bool ensureSettingsDirectory() {
		lexxos::vfs::Stat stat{};
		if (lexxos::vfs::stat(rtcSettingsDirectory, stat)) {
			return stat.type == lexxos::vfs::NodeType::Directory;
		}
		auto const status = lexxos::vfs::createDirectory(rtcSettingsDirectory);
		return status == lexxos::vfs::WriteStatus::Ok;
	}

	RtcSettingsStatus applyConfigLine(RtcSettings& output, std::string_view line) {
		line = trimRightCarriageReturn(line);
		if (line.empty() || startsWith(line, "#")) {
			return RtcSettingsStatus::Ok;
		}

		auto const separator = line.find('=');
		if (separator == std::string_view::npos) {
			return RtcSettingsStatus::Invalid;
		}
		auto const key = line.substr(0, separator);
		auto const value = line.substr(separator + 1U);

		if (key == "enabled") {
			return parseBoolean(value, output.enabled) ? RtcSettingsStatus::Ok : RtcSettingsStatus::Invalid;
		}
		if (key == "prefer-default") {
			return parseBoolean(value, output.preferAsDefault) ? RtcSettingsStatus::Ok : RtcSettingsStatus::Invalid;
		}
		if (key == "auto-sync") {
			return parseBoolean(value, output.autoNetworkSync) ? RtcSettingsStatus::Ok : RtcSettingsStatus::Invalid;
		}
		if (key == "server") {
			return setRtcSettingsServer(output, value) ? RtcSettingsStatus::Ok : RtcSettingsStatus::Invalid;
		}
		if (key == "timeout-ms") {
			return parseUnsigned(value, output.timeoutMs) ? RtcSettingsStatus::Ok : RtcSettingsStatus::Invalid;
		}
		if (key == "interval-ms") {
			return parseUnsigned(value, output.intervalMs) ? RtcSettingsStatus::Ok : RtcSettingsStatus::Invalid;
		}
		if (key == "retry-ms") {
			return parseUnsigned(value, output.retryMs) ? RtcSettingsStatus::Ok : RtcSettingsStatus::Invalid;
		}
		if (key == "startup-delay-ms") {
			return parseUnsigned(value, output.startupDelayMs) ? RtcSettingsStatus::Ok : RtcSettingsStatus::Invalid;
		}
		if (key == "tz") {
			return parseRtcTimezoneOffset(value, output.timezoneOffsetSeconds) ? RtcSettingsStatus::Ok : RtcSettingsStatus::Invalid;
		}
		if (key == "source") {
			return setRtcSettingsSource(output, value) ? RtcSettingsStatus::Ok : RtcSettingsStatus::Invalid;
		}
		if (key == "bus") {
			return setRtcSettingsBus(output, value) ? RtcSettingsStatus::Ok : RtcSettingsStatus::Invalid;
		}
		if (key == "address") {
			if (value.empty() || value == "-") {
				output.addressSet = false;
				output.address = 0;
				return RtcSettingsStatus::Ok;
			}
			std::uint8_t address = 0;
			if (!parseRtcI2cAddress(value, address)) {
				return RtcSettingsStatus::Invalid;
			}
			output.addressSet = true;
			output.address = address;
			return RtcSettingsStatus::Ok;
		}

		return RtcSettingsStatus::Ok;
	}

	RtcSettingsStatus parseConfigFile(lexxos::vfs::FileHandle& file, RtcSettings& output, lexxos::platform::SoftwareRtcConfig const& defaults) {
		resetRtcSettingsDefaults(output, defaults);

		std::array<char, rtcSettingsLineBufferBytes> line{};
		std::size_t lineLength = 0;
		std::uint32_t totalRead = 0;
		for (;;) {
			char character = '\0';
			std::size_t bytesRead = 0;
			auto const readStatus = file.read(&character, 1, bytesRead);
			if (readStatus != lexxos::vfs::ReadStatus::Ok) {
				resetRtcSettingsDefaults(output, defaults);
				return RtcSettingsStatus::IoError;
			}
			if (bytesRead == 0) {
				break;
			}
			if (++totalRead > rtcSettingsMaxFileBytes) {
				resetRtcSettingsDefaults(output, defaults);
				return RtcSettingsStatus::Invalid;
			}
			if (character == '\n') {
				auto const status = applyConfigLine(output, {line.data(), lineLength});
				if (status != RtcSettingsStatus::Ok) {
					resetRtcSettingsDefaults(output, defaults);
					return status;
				}
				lineLength = 0;
				continue;
			}
			if (lineLength + 1U >= line.size()) {
				resetRtcSettingsDefaults(output, defaults);
				return RtcSettingsStatus::Invalid;
			}
			line[lineLength++] = character;
		}

		if (lineLength != 0) {
			auto const status = applyConfigLine(output, {line.data(), lineLength});
			if (status != RtcSettingsStatus::Ok) {
				resetRtcSettingsDefaults(output, defaults);
				return status;
			}
		}

		std::string_view error{};
		return validateRtcSettings(output, error) ? RtcSettingsStatus::Ok : RtcSettingsStatus::Invalid;
	}

} // namespace

std::string_view toString(RtcSettingsStatus status) {
	switch (status) {
		case RtcSettingsStatus::Ok:
			return "ok";
		case RtcSettingsStatus::NotFound:
			return "not-found";
		case RtcSettingsStatus::Invalid:
			return "invalid";
		case RtcSettingsStatus::IoError:
			return "io-error";
	}
	return "unknown";
}

std::string_view rtcSettingsServer(RtcSettings const& settings) {
	return arrayString(settings.server);
}

std::string_view rtcSettingsSource(RtcSettings const& settings) {
	return arrayString(settings.source);
}

std::string_view rtcSettingsBus(RtcSettings const& settings) {
	return arrayString(settings.bus);
}

bool setRtcSettingsServer(RtcSettings& settings, std::string_view server) {
	if (server.empty()) {
		return false;
	}
	for (char const character : server) {
		if (character <= 0x20 || character == '\x7F') {
			return false;
		}
	}
	return copyString(server, settings.server);
}

bool isSupportedRtcSource(std::string_view source) {
	return source == "auto"
	    || source == "software"
	    || source == "soft"
	    || source == "picocalc"
	    || source == "ds3231"
	    || source == "pcf8563"
	    || source == "bm8563";
}

bool setRtcSettingsSource(RtcSettings& settings, std::string_view source) {
	if (!isSupportedRtcSource(source)) {
		return false;
	}
	if (source == "soft") {
		source = "software";
	} else if (source == "bm8563") {
		source = "pcf8563";
	}
	return copyString(source, settings.source);
}

bool setRtcSettingsBus(RtcSettings& settings, std::string_view bus) {
	if (bus.empty()) {
		return false;
	}
	for (char const character : bus) {
		if (character <= 0x20 || character == '\x7F') {
			return false;
		}
	}
	return copyString(bus, settings.bus);
}

bool parseRtcTimezoneOffset(std::string_view text, std::int32_t& output) {
	if (text.empty()
	    || text == "UTC"
	    || text == "utc"
	    || text == "Z"
	    || text == "z") {
		output = 0;
		return true;
	}

	if (text.size() < 2U || (text[0] != '+' && text[0] != '-')) {
		return false;
	}

	bool const negative = text[0] == '-';
	auto const value = text.substr(1U);
	std::uint32_t hours = 0;
	std::uint32_t minutes = 0;
	auto const separator = value.find(':');
	if (separator != std::string_view::npos) {
		if (separator == 0U || separator > 2U || value.size() != separator + 3U) {
			return false;
		}
		if (!parseSmallUnsigned(value.substr(0U, separator), hours)
		    || !parseSmallUnsigned(value.substr(separator + 1U), minutes)) {
			return false;
		}
	} else if (value.size() <= 2U) {
		if (!parseSmallUnsigned(value, hours)) {
			return false;
		}
	} else if (value.size() == 4U) {
		if (!parseSmallUnsigned(value.substr(0U, 2U), hours)
		    || !parseSmallUnsigned(value.substr(2U, 2U), minutes)) {
			return false;
		}
	} else {
		return false;
	}

	if (hours > 23U || minutes > 59U) {
		return false;
	}

	std::int32_t const seconds = static_cast<std::int32_t>(hours * 3600U + minutes * 60U);
	output = negative ? -seconds : seconds;
	return true;
}

void formatRtcTimezoneOffset(std::int32_t offsetSeconds, char (&output)[10]) {
	output[0] = 'U';
	output[1] = 'T';
	output[2] = 'C';
	output[3] = '\0';
	if (offsetSeconds == 0) {
		return;
	}

	std::uint32_t const absolute = static_cast<std::uint32_t>(offsetSeconds < 0 ? -offsetSeconds : offsetSeconds);
	std::uint32_t const hours = absolute / 3600U;
	std::uint32_t const minutes = (absolute / 60U) % 60U;
	output[0] = offsetSeconds < 0 ? '-' : '+';
	output[1] = static_cast<char>('0' + (hours / 10U));
	output[2] = static_cast<char>('0' + (hours % 10U));
	output[3] = ':';
	output[4] = static_cast<char>('0' + (minutes / 10U));
	output[5] = static_cast<char>('0' + (minutes % 10U));
	output[6] = '\0';
}

bool parseRtcI2cAddress(std::string_view text, std::uint8_t& output) {
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
	} else if (!parseUnsigned(text, value)) {
		return false;
	}

	if (value > 0x7FU) {
		return false;
	}
	output = static_cast<std::uint8_t>(value);
	return true;
}

void resetRtcSettingsDefaults(RtcSettings& output, lexxos::platform::SoftwareRtcConfig const& config) {
	output = RtcSettings{};
	output.enabled = config.enabled;
	output.preferAsDefault = config.preferAsDefault;
	output.autoNetworkSync = config.autoNetworkSync;
	output.timeoutMs = config.networkSyncTimeoutMs;
	output.intervalMs = config.networkSyncIntervalMs;
	output.retryMs = config.networkSyncRetryIntervalMs;
	output.startupDelayMs = config.startupDelayMs;
	output.timezoneOffsetSeconds = config.timezoneOffsetSeconds;
	(void) setRtcSettingsServer(output, config.networkSyncServer != nullptr ? config.networkSyncServer : "pool.ntp.org");
	(void) setRtcSettingsSource(output, "auto");
	(void) setRtcSettingsBus(output, "debug");
	output.addressSet = false;
	output.address = 0;
}

bool validateRtcSettings(RtcSettings const& settings, std::string_view& error) {
	if (rtcSettingsServer(settings).empty()) {
		error = "server is empty";
		return false;
	}
	if (settings.timeoutMs == 0 || settings.timeoutMs > rtcSettingsMaxTimeoutMs) {
		error = "timeout is invalid";
		return false;
	}
	if (!isSupportedRtcSource(rtcSettingsSource(settings))) {
		error = "source is invalid";
		return false;
	}
	if (rtcSettingsBus(settings).empty()) {
		error = "bus is empty";
		return false;
	}
	return true;
}

RtcSettingsStatus loadRtcSettings(RtcSettings& output, lexxos::platform::SoftwareRtcConfig const& defaults) {
	lexxos::vfs::FileHandle file;
	auto const openStatus = lexxos::vfs::open(rtcSettingsPath, lexxos::vfs::OpenMode::Read, file);
	if (openStatus == lexxos::vfs::OpenStatus::NotFound) {
		resetRtcSettingsDefaults(output, defaults);
		return RtcSettingsStatus::NotFound;
	}
	if (openStatus != lexxos::vfs::OpenStatus::Ok) {
		resetRtcSettingsDefaults(output, defaults);
		return RtcSettingsStatus::IoError;
	}

	return parseConfigFile(file, output, defaults);
}

RtcSettingsStatus saveRtcSettings(RtcSettings const& settings) {
	std::string_view error{};
	if (!validateRtcSettings(settings, error)) {
		return RtcSettingsStatus::Invalid;
	}
	if (!ensureSettingsDirectory()) {
		return RtcSettingsStatus::IoError;
	}

	char timezone[10]{};
	formatRtcTimezoneOffset(settings.timezoneOffsetSeconds, timezone);

	lexxos::vfs::FileHandle file;
	auto const openStatus = lexxos::vfs::open(rtcSettingsPath, lexxos::vfs::OpenMode::CreateTruncateWrite, file);
	if (openStatus != lexxos::vfs::OpenStatus::Ok) {
		return RtcSettingsStatus::IoError;
	}

	bool ok = appendText(file, "# LexxOS RTC settings\n")
	    && appendLine(file, "enabled", settings.enabled)
	    && appendLine(file, "prefer-default", settings.preferAsDefault)
	    && appendLine(file, "auto-sync", settings.autoNetworkSync)
	    && appendLine(file, "server", rtcSettingsServer(settings))
	    && appendLine(file, "timeout-ms", settings.timeoutMs)
	    && appendLine(file, "interval-ms", settings.intervalMs)
	    && appendLine(file, "retry-ms", settings.retryMs)
	    && appendLine(file, "startup-delay-ms", settings.startupDelayMs)
	    && appendLine(file, "tz", std::string_view{timezone})
	    && appendLine(file, "source", rtcSettingsSource(settings))
	    && appendLine(file, "bus", rtcSettingsBus(settings))
	    && appendText(file, "address=");
	if (ok && settings.addressSet) {
		ok = appendAddress(file, settings.address);
	} else if (ok) {
		ok = appendText(file, "-");
	}
	ok = ok && appendText(file, "\n");
	if (!ok) {
		return RtcSettingsStatus::IoError;
	}
	return RtcSettingsStatus::Ok;
}

RtcSettingsStatus clearRtcSettings() {
	auto const status = lexxos::vfs::remove(rtcSettingsPath);
	if (status == lexxos::vfs::RemoveStatus::Ok || status == lexxos::vfs::RemoveStatus::NotFound) {
		return RtcSettingsStatus::Ok;
	}
	return RtcSettingsStatus::IoError;
}

lexxos::platform::SoftwareRtcConfig softwareRtcConfigFromSettings(RtcSettings const& settings) {
	return {
	    .enabled = settings.enabled,
	    .preferAsDefault = settings.preferAsDefault,
	    .autoNetworkSync = settings.autoNetworkSync,
	    .networkSyncServer = settings.server.data(),
	    .networkSyncTimeoutMs = settings.timeoutMs,
	    .networkSyncIntervalMs = settings.intervalMs,
	    .networkSyncRetryIntervalMs = settings.retryMs,
	    .startupDelayMs = settings.startupDelayMs,
	    .timezoneOffsetSeconds = settings.timezoneOffsetSeconds,
	};
}

} // namespace lexxos::apps::rtc_programs::detail
