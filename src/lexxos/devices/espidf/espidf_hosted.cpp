#include <lexxos/devices/espidf/espidf_hosted.h>

#include <cstddef>
#include <cstdio>
#include <string_view>

#include <esp_err.h>
#include <esp_hosted.h>
#include <esp_hosted_host_fw_ver.h>
#include <lexxos/devices/espidf/espidf_wifi.h>

namespace lexxos::devices::espidf_hosted {
namespace {

	void copyFixedString(char* output, std::size_t outputSize, char const* input, std::size_t inputSize) {
		if (output == nullptr || outputSize == 0U) {
			return;
		}
		std::size_t length = 0;
		while (length + 1U < outputSize && length < inputSize && input[length] != '\0') {
			output[length] = input[length];
			++length;
		}
		output[length] = '\0';
	}

	void setErrorName(char (&output)[32], esp_err_t error) {
		auto const* name = esp_err_to_name(error);
		std::snprintf(output, sizeof(output), "%s", name != nullptr ? name : "ESP_ERR_UNKNOWN");
	}

	int compareVersions(Version host, Version coprocessor) {
		if (host.major != coprocessor.major) {
			return host.major > coprocessor.major ? 1 : -1;
		}
		if (host.minor != coprocessor.minor) {
			return host.minor > coprocessor.minor ? 1 : -1;
		}
		if (host.patch != coprocessor.patch) {
			return host.patch > coprocessor.patch ? 1 : -1;
		}
		return 0;
	}

} // namespace

bool isHostedBoard(lexxos::platform::BoardDescriptor const& board) {
	if (!board.wireless.wifi || board.wireless.backend == nullptr) {
		return false;
	}
	return std::string_view{board.wireless.backend}.find("esp-hosted") != std::string_view::npos;
}

bool activateSupported(Version version) {
	return version.valid && (version.major > 2U || (version.major == 2U && version.minor > 5U));
}

Info readInfo(lexxos::platform::BoardDescriptor const& board, bool startWifiBackend) {
	Info info{};
	info.supported = isHostedBoard(board);
	info.hostVersion = {
	    .major = ESP_HOSTED_VERSION_MAJOR_1,
	    .minor = ESP_HOSTED_VERSION_MINOR_1,
	    .patch = ESP_HOSTED_VERSION_PATCH_1,
	    .valid = true,
	};

	if (!info.supported) {
		info.backendStartError = ESP_ERR_NOT_SUPPORTED;
		info.versionError = ESP_ERR_NOT_SUPPORTED;
		info.appError = ESP_ERR_NOT_SUPPORTED;
		setErrorName(info.backendStartErrorName, ESP_ERR_NOT_SUPPORTED);
		setErrorName(info.versionErrorName, ESP_ERR_NOT_SUPPORTED);
		setErrorName(info.appErrorName, ESP_ERR_NOT_SUPPORTED);
		return info;
	}

	info.backendStartRequested = startWifiBackend;
	if (startWifiBackend) {
		info.backendStarted = lexxos::devices::espidf_wifi::start(board);
		auto const status = lexxos::devices::espidf_wifi::status();
		info.backendStartError = status.lastError;
		std::snprintf(info.backendStartErrorName,
		    sizeof(info.backendStartErrorName),
		    "%s",
		    status.lastErrorName[0] != '\0' ? status.lastErrorName : "ESP_OK");
		if (!info.backendStarted) {
			info.versionError = info.backendStartError != 0 ? info.backendStartError : ESP_FAIL;
			info.appError = info.versionError;
			setErrorName(info.versionErrorName, static_cast<esp_err_t>(info.versionError));
			setErrorName(info.appErrorName, static_cast<esp_err_t>(info.appError));
			return info;
		}
	} else {
		info.backendStarted = lexxos::devices::espidf_wifi::status().started;
		setErrorName(info.backendStartErrorName, ESP_OK);
		if (!info.backendStarted) {
			info.versionError = ESP_ERR_INVALID_STATE;
			info.appError = ESP_ERR_INVALID_STATE;
			setErrorName(info.versionErrorName, ESP_ERR_INVALID_STATE);
			setErrorName(info.appErrorName, ESP_ERR_INVALID_STATE);
			return info;
		}
	}

	esp_hosted_coprocessor_fwver_t version{};
	auto result = esp_hosted_get_coprocessor_fwversion(&version);
	info.versionError = result;
	setErrorName(info.versionErrorName, result);
	if (result == ESP_OK) {
		info.coprocessorVersion = {
		    .major = version.major1,
		    .minor = version.minor1,
		    .patch = version.patch1,
		    .valid = true,
		};
		info.compatibilityKnown = true;
		info.majorMinorCompatible = info.hostVersion.major == info.coprocessorVersion.major
		    && info.hostVersion.minor == info.coprocessorVersion.minor;
		info.versionComparison = compareVersions(info.hostVersion, info.coprocessorVersion);
	}

	esp_hosted_app_desc_t app{};
	result = esp_hosted_get_coprocessor_app_desc(&app);
	info.appError = result;
	setErrorName(info.appErrorName, result);
	if (result == ESP_OK) {
		info.app.magicWord = app.magic_word;
		info.app.magicValid = app.magic_word == ESP_HOSTED_APP_DESC_MAGIC_WORD;
		info.app.secureVersion = app.secure_version;
		copyFixedString(info.app.projectName, sizeof(info.app.projectName), app.project_name, sizeof(app.project_name));
		copyFixedString(info.app.version, sizeof(info.app.version), app.version, sizeof(app.version));
		copyFixedString(info.app.idfVersion, sizeof(info.app.idfVersion), app.idf_ver, sizeof(app.idf_ver));
		copyFixedString(info.app.buildDate, sizeof(info.app.buildDate), app.date, sizeof(app.date));
		copyFixedString(info.app.buildTime, sizeof(info.app.buildTime), app.time, sizeof(app.time));
		info.app.valid = info.app.magicValid || info.app.projectName[0] != '\0' || info.app.version[0] != '\0' || info.app.idfVersion[0] != '\0';
	}

	return info;
}

char const* errorName(int error) {
	return esp_err_to_name(static_cast<esp_err_t>(error));
}

} // namespace lexxos::devices::espidf_hosted
