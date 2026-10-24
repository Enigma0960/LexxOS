#pragma once

#include <cstdint>

namespace lexxos::devices::rtc {

struct DateTime {
	std::uint16_t year{2000};
	std::uint8_t month{1};
	std::uint8_t day{1};
	std::uint8_t weekday{6};
	std::uint8_t hour{0};
	std::uint8_t minute{0};
	std::uint8_t second{0};
};

[[nodiscard]] bool isLeapYear(std::uint16_t year);
[[nodiscard]] bool isValidDate(DateTime const& value);
[[nodiscard]] std::uint8_t daysInMonth(std::uint16_t year, std::uint8_t month);
[[nodiscard]] std::uint8_t weekdayIso(DateTime const& value);
[[nodiscard]] bool unixSecondsToDateTime(std::uint64_t unixSeconds, DateTime& output);
[[nodiscard]] bool dateTimeToUnixSeconds(DateTime const& value, std::uint32_t& output);

} // namespace lexxos::devices::rtc
