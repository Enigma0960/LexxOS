#include <lexxos/apps/storage/detail/sd_settings_support.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/devices/sd_card.h>

namespace lexxos::apps::storage_programs::detail {
namespace {

	constexpr std::uint32_t fallbackSdInitializationClockHz = 400000;
	constexpr std::uint32_t fallbackSdSafeClockHz = 1000000;
	constexpr std::uint32_t fallbackSdDefaultClockHz = 50000000;
	constexpr std::uint32_t fallbackSdPerformanceClockHz = 60000000;
	constexpr std::uint32_t fallbackSdMaxClockHz = 62000000;

	[[nodiscard]] std::uint32_t clockOrFallback(std::uint32_t value, std::uint32_t fallback) {
		return value == 0 ? fallback : value;
	}

	[[nodiscard]] lexxos::devices::SdTimingProfile timingProfile() {
		return lexxos::devices::sd_card::timingProfile();
	}

} // namespace

std::uint32_t sdSpeedMinimumBaudRate() {
	auto const profile = timingProfile();
	return clockOrFallback(profile.initializationClockHz, fallbackSdInitializationClockHz);
}

std::uint32_t sdSafeBaudRate() {
	auto const profile = timingProfile();
	return clockOrFallback(profile.safeClockHz, fallbackSdSafeClockHz);
}

std::uint32_t sdStableBaudRate() {
	auto const profile = timingProfile();
	return clockOrFallback(profile.defaultClockHz, fallbackSdDefaultClockHz);
}

std::uint32_t sdPerformanceBaudRate() {
	auto const profile = timingProfile();
	return clockOrFallback(profile.performanceClockHz, fallbackSdPerformanceClockHz);
}

std::uint32_t sdMaximumBaudRate() {
	auto const profile = timingProfile();
	return clockOrFallback(profile.maxClockHz, fallbackSdMaxClockHz);
}

bool parseSpiMode(std::string_view text, lexxos::devices::SpiClockMode& output) {
	std::uint32_t value = 0;
	if (!parseUnsigned32(text, value) || value > 3) {
		return false;
	}
	output = static_cast<lexxos::devices::SpiClockMode>(value);
	return true;
}

bool parseOnOff(std::string_view text, bool& output) {
	if (text == "on" || text == "1" || text == "true") {
		output = true;
		return true;
	}
	if (text == "off" || text == "0" || text == "false") {
		output = false;
		return true;
	}
	return false;
}

} // namespace lexxos::apps::storage_programs::detail
