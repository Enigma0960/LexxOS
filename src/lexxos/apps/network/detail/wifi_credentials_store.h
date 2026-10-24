#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace lexxos::apps::network_programs::detail {

inline constexpr std::string_view wifiConfigPath = "/mnt/sd/.lexx/wifi.conf";

struct WifiCredentials {
	bool enabled{true};
	std::array<char, 33> ssid{};
	std::array<char, 65> password{};
	std::array<char, 16> auth{};
	std::uint32_t timeoutMs{30000};
};

enum class WifiCredentialsStatus : std::uint8_t {
	Ok,
	NotFound,
	Invalid,
	IoError,
	Disabled,
};

[[nodiscard]] std::string_view toString(WifiCredentialsStatus status);
[[nodiscard]] std::string_view wifiCredentialsPath();
[[nodiscard]] std::string_view wifiCredentialsSsid(WifiCredentials const& credentials);
[[nodiscard]] std::string_view wifiCredentialsPassword(WifiCredentials const& credentials);
[[nodiscard]] std::string_view wifiCredentialsAuth(WifiCredentials const& credentials);
[[nodiscard]] bool isSupportedWifiAuth(std::string_view auth);

[[nodiscard]] bool setWifiCredentialsSsid(WifiCredentials& credentials, std::string_view ssid);
[[nodiscard]] bool setWifiCredentialsPassword(WifiCredentials& credentials, std::string_view password);
[[nodiscard]] bool setWifiCredentialsAuth(WifiCredentials& credentials, std::string_view auth);
[[nodiscard]] bool validateWifiCredentials(WifiCredentials const& credentials, std::string_view& error);

[[nodiscard]] WifiCredentialsStatus loadWifiCredentials(WifiCredentials& output);
[[nodiscard]] WifiCredentialsStatus saveWifiCredentials(WifiCredentials const& credentials);
[[nodiscard]] WifiCredentialsStatus clearWifiCredentials();

} // namespace lexxos::apps::network_programs::detail
