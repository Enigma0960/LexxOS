#include <lexxos/devices/rtc/date_time.h>

#include <limits>

namespace lexxos::devices::rtc {
namespace {

	std::int64_t daysFromCivil(std::uint16_t year, std::uint8_t month, std::uint8_t day) {
		std::int64_t y = year;
		y -= month <= 2U ? 1 : 0;
		std::int64_t const era = (y >= 0 ? y : y - 399) / 400;
		auto const yearOfEra = static_cast<std::uint32_t>(y - era * 400);
		auto const monthValue = static_cast<std::uint32_t>(month);
		auto const dayOfYear = static_cast<std::uint32_t>((153U * (monthValue + (monthValue > 2U ? 0U : 12U) - 3U) + 2U) / 5U + day - 1U);
		auto const dayOfEra = static_cast<std::uint32_t>(yearOfEra * 365U + yearOfEra / 4U - yearOfEra / 100U + dayOfYear);
		return era * 146097 + static_cast<std::int64_t>(dayOfEra) - 719468;
	}

} // namespace

bool isLeapYear(std::uint16_t year) {
	return (year % 4U == 0U && year % 100U != 0U) || year % 400U == 0U;
}

std::uint8_t daysInMonth(std::uint16_t year, std::uint8_t month) {
	switch (month) {
		case 1:
		case 3:
		case 5:
		case 7:
		case 8:
		case 10:
		case 12:
			return 31;
		case 4:
		case 6:
		case 9:
		case 11:
			return 30;
		case 2:
			return isLeapYear(year) ? 29 : 28;
		default:
			return 0;
	}
}

bool isValidDate(DateTime const& value) {
	if (value.year < 2000U || value.year > 2199U) {
		return false;
	}
	if (value.month < 1U || value.month > 12U) {
		return false;
	}
	if (value.day < 1U || value.day > daysInMonth(value.year, value.month)) {
		return false;
	}
	if (value.weekday > 7U) {
		return false;
	}
	if (value.hour > 23U || value.minute > 59U || value.second > 59U) {
		return false;
	}
	return true;
}

std::uint8_t weekdayIso(DateTime const& value) {
	std::uint16_t year = value.year;
	std::uint8_t month = value.month;
	std::uint8_t const day = value.day;
	if (month < 3U) {
		--year;
		month = static_cast<std::uint8_t>(month + 12U);
	}

	std::uint16_t const k = static_cast<std::uint16_t>(year % 100U);
	std::uint16_t const j = static_cast<std::uint16_t>(year / 100U);
	std::uint16_t const h = static_cast<std::uint16_t>((day + ((13U * (month + 1U)) / 5U) + k + (k / 4U) + (j / 4U) + (5U * j)) % 7U);
	if (h == 0U) {
		return 6U;
	}
	if (h == 1U) {
		return 7U;
	}
	return static_cast<std::uint8_t>(h - 1U); // ISO-like: Monday=1, Sunday=7.
}

bool unixSecondsToDateTime(std::uint64_t unixSeconds, DateTime& output) {
	std::int64_t daysSinceUnixEpoch = static_cast<std::int64_t>(unixSeconds / 86400ULL);
	std::uint32_t const secondsOfDay = static_cast<std::uint32_t>(unixSeconds % 86400ULL);

	std::int64_t z = daysSinceUnixEpoch + 719468;
	std::int64_t const era = (z >= 0 ? z : z - 146096) / 146097;
	std::uint64_t const dayOfEra = static_cast<std::uint64_t>(z - era * 146097);
	std::uint64_t const yearOfEra = (dayOfEra - dayOfEra / 1460U + dayOfEra / 36524U - dayOfEra / 146096U) / 365U;
	std::int64_t year = static_cast<std::int64_t>(yearOfEra) + era * 400;
	std::uint64_t const dayOfYear = dayOfEra - (365U * yearOfEra + yearOfEra / 4U - yearOfEra / 100U);
	std::uint64_t const monthPrime = (5U * dayOfYear + 2U) / 153U;
	std::uint64_t const day = dayOfYear - (153U * monthPrime + 2U) / 5U + 1U;
	std::uint64_t const month = monthPrime < 10U ? monthPrime + 3U : monthPrime - 9U;
	year += month <= 2U ? 1 : 0;

	if (year < 0 || year > std::numeric_limits<std::uint16_t>::max()) {
		return false;
	}

	DateTime value{};
	value.year = static_cast<std::uint16_t>(year);
	value.month = static_cast<std::uint8_t>(month);
	value.day = static_cast<std::uint8_t>(day);
	value.hour = static_cast<std::uint8_t>(secondsOfDay / 3600U);
	value.minute = static_cast<std::uint8_t>((secondsOfDay / 60U) % 60U);
	value.second = static_cast<std::uint8_t>(secondsOfDay % 60U);
	value.weekday = weekdayIso(value);
	if (!isValidDate(value)) {
		return false;
	}

	output = value;
	return true;
}

bool dateTimeToUnixSeconds(DateTime const& value, std::uint32_t& output) {
	if (!isValidDate(value)) {
		return false;
	}

	std::int64_t const days = daysFromCivil(value.year, value.month, value.day);
	if (days < 0) {
		return false;
	}

	std::uint64_t const seconds = static_cast<std::uint64_t>(days) * 86400ULL
	    + static_cast<std::uint64_t>(value.hour) * 3600ULL
	    + static_cast<std::uint64_t>(value.minute) * 60ULL
	    + static_cast<std::uint64_t>(value.second);
	if (seconds > std::numeric_limits<std::uint32_t>::max()) {
		return false;
	}

	output = static_cast<std::uint32_t>(seconds);
	return true;
}

} // namespace lexxos::devices::rtc
