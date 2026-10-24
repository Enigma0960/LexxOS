#include <lexxos/apps/network/detail/wifi_credentials_store.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::network_programs::detail {
namespace {

	constexpr std::string_view wifiConfigDirectory = "/mnt/sd/.lexx";
	constexpr std::string_view wifiLegacyConfigPath = "/mnt/sd/lexxos/wifi.conf";
	constexpr std::uint32_t wifiDefaultTimeoutMs = 30000;
	constexpr std::uint32_t wifiMaxTimeoutMs = 120000;
	constexpr std::size_t wifiConfigBufferBytes = 512;

	using ConfigBuffer = std::array<char, wifiConfigBufferBytes>;

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

	bool appendText(ConfigBuffer& buffer, std::size_t& length, std::string_view text) {
		if (length + text.size() >= buffer.size()) {
			return false;
		}
		std::memcpy(buffer.data() + length, text.data(), text.size());
		length += text.size();
		buffer[length] = '\0';
		return true;
	}

	bool appendUnsigned(ConfigBuffer& buffer, std::size_t& length, std::uint32_t value) {
		char text[11]{};
		std::size_t index = sizeof(text) - 1U;
		text[index] = '\0';
		do {
			text[--index] = static_cast<char>('0' + (value % 10U));
			value /= 10U;
		} while (value != 0 && index > 0);
		return appendText(buffer, length, {text + index, sizeof(text) - 1U - index});
	}

	bool appendLine(ConfigBuffer& buffer, std::size_t& length, std::string_view key, std::string_view value) {
		return appendText(buffer, length, key)
		    && appendText(buffer, length, "=")
		    && appendText(buffer, length, value)
		    && appendText(buffer, length, "\n");
	}

	bool appendLine(ConfigBuffer& buffer, std::size_t& length, std::string_view key, std::uint32_t value) {
		return appendText(buffer, length, key)
		    && appendText(buffer, length, "=")
		    && appendUnsigned(buffer, length, value)
		    && appendText(buffer, length, "\n");
	}

	void resetDefaults(WifiCredentials& output) {
		output = WifiCredentials{};
		output.enabled = true;
		output.timeoutMs = wifiDefaultTimeoutMs;
		(void) copyString("auto", output.auth);
	}

	WifiCredentialsStatus applyConfigLine(WifiCredentials& output, std::string_view line) {
		line = trimRightCarriageReturn(line);
		if (line.empty() || startsWith(line, "#")) {
			return WifiCredentialsStatus::Ok;
		}

		auto const separator = line.find('=');
		if (separator == std::string_view::npos) {
			return WifiCredentialsStatus::Invalid;
		}
		auto const key = line.substr(0, separator);
		auto const value = line.substr(separator + 1U);

		if (key == "enabled") {
			bool enabled = false;
			if (!parseBoolean(value, enabled)) {
				return WifiCredentialsStatus::Invalid;
			}
			output.enabled = enabled;
			return WifiCredentialsStatus::Ok;
		}
		if (key == "ssid") {
			return copyString(value, output.ssid) ? WifiCredentialsStatus::Ok : WifiCredentialsStatus::Invalid;
		}
		if (key == "password") {
			return copyString(value, output.password) ? WifiCredentialsStatus::Ok : WifiCredentialsStatus::Invalid;
		}
		if (key == "auth") {
			return copyString(value, output.auth) ? WifiCredentialsStatus::Ok : WifiCredentialsStatus::Invalid;
		}
		if (key == "timeout") {
			std::uint32_t timeoutMs = 0;
			if (!parseUnsigned(value, timeoutMs)) {
				return WifiCredentialsStatus::Invalid;
			}
			output.timeoutMs = timeoutMs;
			return WifiCredentialsStatus::Ok;
		}

		return WifiCredentialsStatus::Ok;
	}

	WifiCredentialsStatus parseConfig(std::string_view text, WifiCredentials& output) {
		resetDefaults(output);

		std::size_t offset = 0;
		while (offset <= text.size()) {
			auto const newline = text.find('\n', offset);
			auto const end = newline == std::string_view::npos ? text.size() : newline;
			auto const status = applyConfigLine(output, text.substr(offset, end - offset));
			if (status != WifiCredentialsStatus::Ok) {
				return status;
			}
			if (newline == std::string_view::npos) {
				break;
			}
			offset = newline + 1U;
		}

		std::string_view error{};
		if (!validateWifiCredentials(output, error)) {
			return output.enabled ? WifiCredentialsStatus::Invalid : WifiCredentialsStatus::Disabled;
		}
		return output.enabled ? WifiCredentialsStatus::Ok : WifiCredentialsStatus::Disabled;
	}

	bool ensureConfigDirectory() {
		lexxos::vfs::Stat stat{};
		if (lexxos::vfs::stat(wifiConfigDirectory, stat)) {
			return stat.type == lexxos::vfs::NodeType::Directory;
		}
		auto const status = lexxos::vfs::createDirectory(wifiConfigDirectory);
		return status == lexxos::vfs::WriteStatus::Ok;
	}

	WifiCredentialsStatus loadWifiCredentialsFromPath(std::string_view path, WifiCredentials& output) {
		resetDefaults(output);

		lexxos::vfs::FileHandle file;
		auto const openStatus = lexxos::vfs::open(path, lexxos::vfs::OpenMode::Read, file);
		if (openStatus == lexxos::vfs::OpenStatus::NotFound) {
			return WifiCredentialsStatus::NotFound;
		}
		if (openStatus != lexxos::vfs::OpenStatus::Ok) {
			return WifiCredentialsStatus::IoError;
		}

		ConfigBuffer buffer{};
		std::size_t totalRead = 0;
		while (totalRead < buffer.size() - 1U) {
			std::size_t bytesRead = 0;
			auto const readStatus = file.read(buffer.data() + totalRead, buffer.size() - 1U - totalRead, bytesRead);
			if (readStatus != lexxos::vfs::ReadStatus::Ok) {
				return WifiCredentialsStatus::IoError;
			}
			if (bytesRead == 0) {
				break;
			}
			totalRead += bytesRead;
		}
		buffer[totalRead] = '\0';

		if (totalRead == buffer.size() - 1U && file.position() < file.size()) {
			return WifiCredentialsStatus::Invalid;
		}
		return parseConfig({buffer.data(), totalRead}, output);
	}

} // namespace

std::string_view toString(WifiCredentialsStatus status) {
	switch (status) {
		case WifiCredentialsStatus::Ok:
			return "ok";
		case WifiCredentialsStatus::NotFound:
			return "not-found";
		case WifiCredentialsStatus::Invalid:
			return "invalid";
		case WifiCredentialsStatus::IoError:
			return "io-error";
		case WifiCredentialsStatus::Disabled:
			return "disabled";
	}
	return "unknown";
}

std::string_view wifiCredentialsPath() {
	return wifiConfigPath;
}

std::string_view wifiCredentialsSsid(WifiCredentials const& credentials) {
	return arrayString(credentials.ssid);
}

std::string_view wifiCredentialsPassword(WifiCredentials const& credentials) {
	return arrayString(credentials.password);
}

std::string_view wifiCredentialsAuth(WifiCredentials const& credentials) {
	return arrayString(credentials.auth);
}

bool isSupportedWifiAuth(std::string_view auth) {
	return auth == "auto" || auth == "open" || auth == "wpa" || auth == "wpa2" || auth == "mixed";
}

bool setWifiCredentialsSsid(WifiCredentials& credentials, std::string_view ssid) {
	return copyString(ssid, credentials.ssid);
}

bool setWifiCredentialsPassword(WifiCredentials& credentials, std::string_view password) {
	return copyString(password, credentials.password);
}

bool setWifiCredentialsAuth(WifiCredentials& credentials, std::string_view auth) {
	return copyString(auth, credentials.auth);
}

bool validateWifiCredentials(WifiCredentials const& credentials, std::string_view& error) {
	auto const ssid = wifiCredentialsSsid(credentials);
	auto const password = wifiCredentialsPassword(credentials);
	auto const auth = wifiCredentialsAuth(credentials);

	if (!credentials.enabled) {
		return true;
	}
	if (ssid.empty()) {
		error = "SSID is empty";
		return false;
	}
	if (!isSupportedWifiAuth(auth)) {
		error = "auth must be auto, open, wpa, wpa2 or mixed";
		return false;
	}
	if (credentials.timeoutMs == 0 || credentials.timeoutMs > wifiMaxTimeoutMs) {
		error = "timeout is invalid";
		return false;
	}
	if (auth != "open" && password.size() < 8U) {
		error = "password must contain at least 8 characters for WPA/WPA2";
		return false;
	}
	return true;
}

WifiCredentialsStatus loadWifiCredentials(WifiCredentials& output) {
	auto status = loadWifiCredentialsFromPath(wifiConfigPath, output);
	if (status != WifiCredentialsStatus::NotFound) {
		return status;
	}
	status = loadWifiCredentialsFromPath(wifiLegacyConfigPath, output);
	if (status == WifiCredentialsStatus::Ok || status == WifiCredentialsStatus::Disabled) {
		if (saveWifiCredentials(output) == WifiCredentialsStatus::Ok) {
			(void) lexxos::vfs::remove(wifiLegacyConfigPath);
		}
	}
	return status;
}

WifiCredentialsStatus saveWifiCredentials(WifiCredentials const& credentials) {
	std::string_view error{};
	if (!validateWifiCredentials(credentials, error)) {
		return WifiCredentialsStatus::Invalid;
	}
	if (!ensureConfigDirectory()) {
		return WifiCredentialsStatus::IoError;
	}

	ConfigBuffer buffer{};
	std::size_t length = 0;
	bool const ok = appendText(buffer, length, "# LexxOS Wi-Fi auto-connect\n")
	    && appendLine(buffer, length, "enabled", credentials.enabled ? "1" : "0")
	    && appendLine(buffer, length, "ssid", wifiCredentialsSsid(credentials))
	    && appendLine(buffer, length, "password", wifiCredentialsPassword(credentials))
	    && appendLine(buffer, length, "auth", wifiCredentialsAuth(credentials))
	    && appendLine(buffer, length, "timeout", credentials.timeoutMs);
	if (!ok) {
		return WifiCredentialsStatus::Invalid;
	}

	auto const status = lexxos::vfs::writeText(wifiConfigPath, {buffer.data(), length}, lexxos::vfs::WriteMode::CreateTruncate);
	return status == lexxos::vfs::WriteStatus::Ok ? WifiCredentialsStatus::Ok : WifiCredentialsStatus::IoError;
}

WifiCredentialsStatus clearWifiCredentials() {
	auto const status = lexxos::vfs::remove(wifiConfigPath);
	auto const legacyStatus = lexxos::vfs::remove(wifiLegacyConfigPath);
	if ((status == lexxos::vfs::RemoveStatus::Ok || status == lexxos::vfs::RemoveStatus::NotFound)
	    && (legacyStatus == lexxos::vfs::RemoveStatus::Ok || legacyStatus == lexxos::vfs::RemoveStatus::NotFound)) {
		return WifiCredentialsStatus::Ok;
	}
	return WifiCredentialsStatus::IoError;
}

} // namespace lexxos::apps::network_programs::detail
