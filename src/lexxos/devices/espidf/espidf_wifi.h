#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/platform/board.h>

namespace lexxos::devices::espidf_wifi {

inline constexpr std::size_t maxWifiScanResults = 32;

enum class AuthMode : std::uint8_t {
	Auto,
	Open,
	Wpa,
	Wpa2,
	Mixed,
};

struct ScanRecord {
	char ssid[33]{};
	std::uint8_t bssid[6]{};
	bool bssidValid{false};
	std::int32_t rssi{0};
	std::uint8_t channel{0};
	char auth[24]{};
};

struct Status {
	bool supported{false};
	bool initialized{false};
	bool staModeEnabled{false};
	bool started{false};
	bool connected{false};
	bool gotIp{false};
	bool macValid{false};
	std::uint8_t mac[6]{};
	bool rssiValid{false};
	std::int32_t rssi{0};
	std::uint8_t signalLevel{0};
	char ssid[33]{};
	char hostname[33]{};
	char ip[16]{};
	char netmask[16]{};
	char gateway[16]{};
	char dns0[16]{};
	char dns1[16]{};
	int lastError{0};
	char lastErrorName[32]{};
	std::uint16_t lastDisconnectReason{0};
};

[[nodiscard]] char const* toString(AuthMode mode);
[[nodiscard]] bool parseAuthMode(std::string_view text, AuthMode& output);

[[nodiscard]] bool start(lexxos::platform::BoardDescriptor const& board);
[[nodiscard]] bool connect(
    lexxos::platform::BoardDescriptor const& board,
    std::string_view ssid,
    std::string_view password,
    AuthMode auth,
    std::uint32_t timeoutMs);
[[nodiscard]] bool disconnect();
[[nodiscard]] bool scan(
    lexxos::platform::BoardDescriptor const& board,
    std::uint32_t timeoutMs,
    ScanRecord* records,
    std::size_t capacity,
    std::size_t& count);
[[nodiscard]] Status status();

} // namespace lexxos::devices::espidf_wifi
