#include <lexxos/apps/rtc/detail/rtc_program_support.h>

#include <lexxos/os/app_context.h>

namespace lexxos::apps::rtc_programs::detail {

namespace {

	using lexxos::devices::rtc::DateTime;

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

} // namespace lexxos::apps::rtc_programs::detail
