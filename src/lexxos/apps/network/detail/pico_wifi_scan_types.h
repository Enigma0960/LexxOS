#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace lexxos::apps::network_programs::pico_detail {

constexpr std::uint32_t wifiDefaultLimit = 24;
constexpr std::uint32_t wifiMaxResults = 32;
constexpr std::size_t wifiBssidLength = 6;

using WifiBssid = std::array<std::uint8_t, wifiBssidLength>;

struct WifiScanRecord {
	char ssid[33]{};
	std::uint8_t ssidLength{0};
	WifiBssid bssid{};
	std::uint16_t channel{0};
	std::uint8_t authMode{0};
	std::int16_t rssi{0};
	std::uint32_t seen{1};
	bool multipleBssid{false};
};

struct WifiScanState {
	std::array<WifiScanRecord, wifiMaxResults> results{};
	std::uint32_t count{0};
	std::uint32_t merged{0};
	std::uint32_t dropped{0};
	std::uint32_t limit{wifiDefaultLimit};
	bool raw{false};
	bool showBssid{false};
	bool armed{false};
};

} // namespace lexxos::apps::network_programs::pico_detail
