#include <lexxos/app/picocalc_display_power.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <lexxos/board_config.h>
#include <lexxos/feature.h>
#include <lexxos/platform/platform_services.h>
#include <lexxos/vfs/vfs.h>

#if FEATURE_ENABLED(DISPLAY) && (BOARD_FAMILY_ENABLED(PICOCALC) || BOARD_FAMILY_ENABLED(JC_ESP32P4_M3_DEV))
	#define LEXXOS_DISPLAY_POWER_POLICY_ENABLED 1
#else
	#define LEXXOS_DISPLAY_POWER_POLICY_ENABLED 0
#endif

#if BOARD_FAMILY_ENABLED(PICOCALC) && FEATURE_ENABLED(DISPLAY)
	#include <lexxos/devices/picocalc/picocalc_southbridge_resource_policy.h>
	#include <lexxos/devices/picocalc/picocalc_southbridge_runtime.h>
#endif

namespace lexxos::app::picocalc_display_power {
namespace {

#if BOARD_FAMILY_ENABLED(JC_ESP32P4_M3_DEV)
	constexpr std::uint8_t fallbackWakeLcdBacklight = 255;
	constexpr std::uint32_t defaultDimTimeoutMs = 300000;
	constexpr std::uint32_t defaultIdleTimeoutMs = 600000;
	constexpr std::uint8_t defaultDimBacklight = 26;
	constexpr std::string_view displayPowerConfigDirectory = "/mnt/sd/.lexx";
	constexpr std::string_view displayPowerConfigSubdirectory = "";
	constexpr std::string_view displayPowerConfigPath = "/mnt/sd/.lexx/display-power.conf";
#else
	#if defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
	constexpr std::uint8_t fallbackWakeLcdBacklight = 3;
	#else
	constexpr std::uint8_t fallbackWakeLcdBacklight = 32;
	#endif
	constexpr std::uint32_t defaultDimTimeoutMs = 0;
	constexpr std::uint32_t defaultIdleTimeoutMs = 300000;
	constexpr std::uint8_t defaultDimBacklight = 26;
	constexpr std::string_view displayPowerConfigDirectory = "/mnt/sd/.lexx";
	constexpr std::string_view displayPowerConfigSubdirectory = "";
	constexpr std::string_view displayPowerConfigPath = "/mnt/sd/.lexx/display-power.conf";
	constexpr std::string_view legacyDisplayPowerConfigPath = "/mnt/sd/lexxos/picocalc/display-power.conf";
#endif

	constexpr std::uint32_t idleCheckIntervalMs = 250;
	constexpr std::uint32_t configRetryIntervalMs = 5000;
	constexpr std::uint32_t maximumIdleTimeoutMs = 86400U * 1000U;

#if LEXXOS_DISPLAY_POWER_POLICY_ENABLED
	enum class IdleStage : std::uint8_t {
		Active,
		Dimmed,
		Off,
	};

	std::uint8_t g_restoreLcdBacklight{fallbackWakeLcdBacklight};
	bool g_restoreLcdBacklightValid{false};
	#if BOARD_FAMILY_ENABLED(PICOCALC) && FEATURE_ENABLED(DISPLAY)
	std::uint8_t g_restoreKeyboardBacklight{0};
	bool g_restoreKeyboardBacklightValid{false};
	#endif
	IdleSettings g_idleSettings{
	    .timeoutMs = defaultIdleTimeoutMs,
	    .dimTimeoutMs = defaultDimTimeoutMs,
	    .dimBacklight = defaultDimBacklight,
	    .mode = lexxos::devices::DisplayPowerMode::Off,
	    .loadedFromConfig = false,
	};
	bool g_configLoaded{false};
	std::uint32_t g_nextConfigLoadMs{0};
	std::uint32_t g_lastActivityMs{0};
	std::uint32_t g_nextIdleCheckMs{0};
	IdleStage g_idleStage{IdleStage::Active};

	[[nodiscard]] std::uint32_t uptimeMs(lexxos::platform::PlatformServices const& platform) {
		auto const& clock = platform.clock;
		return clock.uptimeMs != nullptr ? clock.uptimeMs() : 0U;
	}

	[[nodiscard]] bool timeReached(std::uint32_t now, std::uint32_t target) {
		return static_cast<std::int32_t>(now - target) >= 0;
	}

	[[nodiscard]] bool equalsIgnoreCase(std::string_view left, std::string_view right) {
		if (left.size() != right.size()) {
			return false;
		}
		for (std::size_t index = 0; index < left.size(); ++index) {
			auto const l = left[index] >= 'A' && left[index] <= 'Z' ? static_cast<char>(left[index] - 'A' + 'a') : left[index];
			auto const r = right[index] >= 'A' && right[index] <= 'Z' ? static_cast<char>(right[index] - 'A' + 'a') : right[index];
			if (l != r) {
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] std::string_view trim(std::string_view value) {
		while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r')) {
			value.remove_prefix(1);
		}
		while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) {
			value.remove_suffix(1);
		}
		return value;
	}

	[[nodiscard]] bool parseUnsigned(std::string_view text, std::uint32_t& value) {
		if (text.empty()) {
			return false;
		}
		std::uint32_t parsed = 0;
		for (char const character : text) {
			if (character < '0' || character > '9') {
				return false;
			}
			auto const digit = static_cast<std::uint32_t>(character - '0');
			if (parsed > (0xFFFFFFFFU - digit) / 10U) {
				return false;
			}
			parsed = parsed * 10U + digit;
		}
		value = parsed;
		return true;
	}

	[[nodiscard]] bool parseByte(std::string_view text, std::uint8_t& value) {
		std::uint32_t parsed = 0;
		if (!parseUnsigned(text, parsed) || parsed > 255U) {
			return false;
		}
		value = static_cast<std::uint8_t>(parsed);
		return true;
	}

	[[nodiscard]] bool parsePowerMode(std::string_view text, lexxos::devices::DisplayPowerMode& mode) {
		if (equalsIgnoreCase(text, "off")) {
			mode = lexxos::devices::DisplayPowerMode::Off;
			return true;
		}
		if (equalsIgnoreCase(text, "sleep")) {
			mode = lexxos::devices::DisplayPowerMode::Sleep;
			return true;
		}
		return false;
	}

	[[nodiscard]] bool appendChar(std::array<char, 160>& buffer, std::size_t& offset, char value) {
		if (offset >= buffer.size()) {
			return false;
		}
		buffer[offset++] = value;
		return true;
	}

	[[nodiscard]] bool appendText(std::array<char, 160>& buffer, std::size_t& offset, std::string_view text) {
		if (text.size() > buffer.size() - offset) {
			return false;
		}
		for (char const character : text) {
			buffer[offset++] = character;
		}
		return true;
	}

	[[nodiscard]] bool appendUnsigned(std::array<char, 160>& buffer, std::size_t& offset, std::uint32_t value) {
		char digits[10]{};
		std::size_t digitCount = 0;
		auto remaining = value;
		do {
			digits[digitCount++] = static_cast<char>('0' + remaining % 10U);
			remaining /= 10U;
		} while (remaining != 0);

		while (digitCount > 0) {
			if (!appendChar(buffer, offset, digits[--digitCount])) {
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] bool appendConfigLine(
	    std::array<char, 160>& buffer,
	    std::size_t& offset,
	    std::string_view name,
	    std::uint32_t value) {
		return appendText(buffer, offset, name)
		    && appendChar(buffer, offset, '=')
		    && appendUnsigned(buffer, offset, value)
		    && appendChar(buffer, offset, '\n');
	}

	[[nodiscard]] bool validateSettings(IdleSettings const& settings) {
		if (settings.timeoutMs > maximumIdleTimeoutMs || settings.dimTimeoutMs > maximumIdleTimeoutMs) {
			return false;
		}
		if (settings.dimTimeoutMs != 0 && (settings.timeoutMs == 0 || settings.dimTimeoutMs >= settings.timeoutMs)) {
			return false;
		}
		return settings.mode == lexxos::devices::DisplayPowerMode::Off
		    || settings.mode == lexxos::devices::DisplayPowerMode::Sleep;
	}

	#if BOARD_FAMILY_ENABLED(PICOCALC) && FEATURE_ENABLED(DISPLAY)
	[[nodiscard]] lexxos::devices::PicoCalcSouthbridgeRuntimeService* southbridgeService(
	    lexxos::devices::DeviceManager& devices) {
		return lexxos::devices::PicoCalcSouthbridgeResourcePolicy::readyService(devices);
	}

	[[nodiscard]] lexxos::devices::DeviceResourceLock lockSouthbridge(
	    lexxos::devices::DeviceManager& devices,
	    std::string_view owner,
	    lexxos::devices::DeviceResourcePriority priority) {
		return lexxos::devices::PicoCalcSouthbridgeResourcePolicy::lock(devices, owner, priority);
	}
	#endif

	[[nodiscard]] bool parseConfigText(std::string_view text, IdleSettings& settings) {
		IdleSettings parsed = g_idleSettings;
		bool hasAny = false;

		while (!text.empty()) {
			auto const newline = text.find('\n');
			std::string_view line = newline == std::string_view::npos ? text : text.substr(0, newline);
			text = newline == std::string_view::npos ? std::string_view{} : text.substr(newline + 1);

			line = trim(line);
			if (line.empty() || line.front() == '#') {
				continue;
			}

			auto const separator = line.find('=');
			if (separator == std::string_view::npos) {
				return false;
			}

			auto const name = trim(line.substr(0, separator));
			auto const valueText = trim(line.substr(separator + 1));
			if (name == "timeout-ms" || name == "off-timeout-ms") {
				std::uint32_t timeoutMs = 0;
				if (!parseUnsigned(valueText, timeoutMs)) {
					return false;
				}
				parsed.timeoutMs = timeoutMs;
				hasAny = true;
			} else if (name == "dim-timeout-ms") {
				std::uint32_t timeoutMs = 0;
				if (!parseUnsigned(valueText, timeoutMs)) {
					return false;
				}
				parsed.dimTimeoutMs = timeoutMs;
				hasAny = true;
			} else if (name == "dim-backlight") {
				std::uint8_t backlight = 0;
				if (!parseByte(valueText, backlight)) {
					return false;
				}
				parsed.dimBacklight = backlight;
				hasAny = true;
			} else if (name == "mode") {
				lexxos::devices::DisplayPowerMode mode{};
				if (!parsePowerMode(valueText, mode)) {
					return false;
				}
				parsed.mode = mode;
				hasAny = true;
			} else {
				return false;
			}
		}

		if (!hasAny) {
			return false;
		}
		if (parsed.timeoutMs == 0) {
			parsed.dimTimeoutMs = 0;
		} else if (parsed.dimTimeoutMs >= parsed.timeoutMs) {
			parsed.dimTimeoutMs = 0;
		}
		if (!validateSettings(parsed)) {
			return false;
		}
		parsed.loadedFromConfig = true;
		settings = parsed;
		return true;
	}

	[[nodiscard]] bool readConfigFromPath(std::string_view path, IdleSettings& settings) {
		lexxos::vfs::FileHandle file;
		auto const openStatus = lexxos::vfs::open(path, lexxos::vfs::OpenMode::Read, file);
		if (openStatus != lexxos::vfs::OpenStatus::Ok) {
			return false;
		}

		std::array<char, 192> buffer{};
		std::size_t bytesRead = 0;
		auto const readStatus = file.read(buffer.data(), buffer.size(), bytesRead);
		if (readStatus != lexxos::vfs::ReadStatus::Ok) {
			return false;
		}
		return parseConfigText(std::string_view{buffer.data(), bytesRead}, settings);
	}

	[[nodiscard]] bool readConfig(IdleSettings& settings) {
		if (readConfigFromPath(displayPowerConfigPath, settings)) {
			return true;
		}
	#if BOARD_FAMILY_ENABLED(PICOCALC)
		return readConfigFromPath(legacyDisplayPowerConfigPath, settings);
	#else
		return false;
	#endif
	}

	[[nodiscard]] bool ensureDirectory(std::string_view path) {
		if (path.empty()) {
			return true;
		}
		lexxos::vfs::Stat node{};
		if (lexxos::vfs::stat(path, node)) {
			return node.type == lexxos::vfs::NodeType::Directory;
		}
		auto const status = lexxos::vfs::createDirectory(path);
		return status == lexxos::vfs::WriteStatus::Ok || status == lexxos::vfs::WriteStatus::IsDirectory;
	}

	[[nodiscard]] bool saveConfig(IdleSettings const& settings) {
		if (!validateSettings(settings)
		    || !ensureDirectory(displayPowerConfigDirectory)
		    || !ensureDirectory(displayPowerConfigSubdirectory)) {
			return false;
		}

		std::array<char, 160> buffer{};
		std::size_t size = 0;
		if (!appendText(buffer, size, "# LexxOS display power settings\n")
		    || !appendConfigLine(buffer, size, "dim-timeout-ms", settings.dimTimeoutMs)
		    || !appendConfigLine(buffer, size, "off-timeout-ms", settings.timeoutMs)
		    || !appendConfigLine(buffer, size, "dim-backlight", settings.dimBacklight)
		    || !appendText(buffer, size, "mode=")
		    || !appendText(buffer, size, lexxos::devices::toString(settings.mode))
		    || !appendChar(buffer, size, '\n')) {
			return false;
		}

		auto const writeStatus = lexxos::vfs::writeText(
		    displayPowerConfigPath,
		    std::string_view{buffer.data(), size},
		    lexxos::vfs::WriteMode::CreateTruncate);
		if (writeStatus != lexxos::vfs::WriteStatus::Ok) {
			return false;
		}
		return lexxos::vfs::sync() == lexxos::vfs::SyncStatus::Ok;
	}

	void loadConfigIfDue(std::uint32_t nowMs, bool force) {
		if (!force && g_configLoaded) {
			return;
		}
		if (!force && g_nextConfigLoadMs != 0 && !timeReached(nowMs, g_nextConfigLoadMs)) {
			return;
		}

		IdleSettings settings{};
		if (readConfig(settings)) {
			g_idleSettings = settings;
			g_configLoaded = true;
			return;
		}

		g_nextConfigLoadMs = nowMs + configRetryIntervalMs;
	}

	void rememberDisplayBacklight(lexxos::devices::DisplayDevice& display) {
		if (!display.supportsBacklight()) {
			return;
		}
		auto const value = display.backlight();
		if (value == 0) {
			return;
		}
		if (g_idleStage == IdleStage::Dimmed && g_restoreLcdBacklightValid) {
			return;
		}
		g_restoreLcdBacklight = value;
		g_restoreLcdBacklightValid = true;
	}

	[[nodiscard]] std::uint8_t wakeDisplayBacklightValue(lexxos::devices::DisplayDevice& display) {
		if (g_restoreLcdBacklightValid) {
			return g_restoreLcdBacklight;
		}
		if (display.supportsBacklight()) {
			auto const current = display.backlight();
			if (current > 0) {
				return current;
			}
		}
		return fallbackWakeLcdBacklight;
	}

	bool requestDisplayBacklight(
	    lexxos::devices::DisplayDevice& display,
	    std::uint8_t value,
	    ApplyResult& result) {
		if (!display.supportsBacklight()) {
			return true;
		}
		result.lcdBacklightAttempted = true;
		result.lcdBacklightValue = value;
		result.lcdBacklightValueValid = true;
		result.lcdBacklightOk = display.setBacklight(value);
		if (result.lcdBacklightOk) {
			result.lcdBacklightValue = display.backlight();
		}
		return result.lcdBacklightOk;
	}

	bool dimDisplayBacklight(
	    lexxos::devices::DeviceManager& devices,
	    lexxos::devices::DisplayDevice& display,
	    std::string_view owner,
	    lexxos::devices::DeviceResourcePriority priority) {
		if (!display.supportsBacklight() || display.powerMode() != lexxos::devices::DisplayPowerMode::On) {
			return false;
		}
		auto displayLock = devices.lockDisplayDevice(display.name(), priority, owner);
		if (!displayLock) {
			return false;
		}
		if (!display.supportsBacklight() || display.powerMode() != lexxos::devices::DisplayPowerMode::On) {
			return false;
		}
		auto const current = display.backlight();
		if (current == 0) {
			return false;
		}
		rememberDisplayBacklight(display);
		auto target = g_idleSettings.dimBacklight;
		if (target == 0) {
			target = 1;
		}
		if (current < target) {
			target = current;
		}
		if (!display.setBacklight(target)) {
			return false;
		}
		g_idleStage = IdleStage::Dimmed;
		return true;
	}

	#if BOARD_FAMILY_ENABLED(PICOCALC) && FEATURE_ENABLED(DISPLAY)
	void rememberCurrentBacklight(
	    lexxos::devices::PicoCalcSouthbridgeRuntimeService& service,
	    std::uint32_t nowMs) {
		(void) service.refreshSouthbridgeSnapshot(nowMs);

		lexxos::devices::PicoCalcSouthbridgeSnapshot snapshot{};
		if (!service.southbridgeSnapshot(snapshot)) {
			return;
		}

		if (snapshot.has(lexxos::devices::PicoCalcSouthbridgeSnapshot::validLcdBacklight)
		    && snapshot.lcdBacklight > 0
		    && (g_idleStage != IdleStage::Dimmed || !g_restoreLcdBacklightValid)) {
			g_restoreLcdBacklight = snapshot.lcdBacklight;
			g_restoreLcdBacklightValid = true;
		}
		if (snapshot.has(lexxos::devices::PicoCalcSouthbridgeSnapshot::validKeyboardBacklight)) {
			g_restoreKeyboardBacklight = snapshot.keyboardBacklight;
			g_restoreKeyboardBacklightValid = true;
		}
	}

	[[nodiscard]] std::uint8_t wakeBacklightValue(
	    lexxos::devices::PicoCalcSouthbridgeRuntimeService& service,
	    std::uint32_t nowMs) {
		if (g_restoreLcdBacklightValid) {
			return g_restoreLcdBacklight;
		}

		(void) service.refreshSouthbridgeSnapshot(nowMs);
		lexxos::devices::PicoCalcSouthbridgeSnapshot snapshot{};
		if (service.southbridgeSnapshot(snapshot)
		    && snapshot.has(lexxos::devices::PicoCalcSouthbridgeSnapshot::validLcdBacklight)
		    && snapshot.lcdBacklight > 0) {
			return snapshot.lcdBacklight;
		}
		return fallbackWakeLcdBacklight;
	}

	[[nodiscard]] std::uint8_t wakeKeyboardBacklightValue(
	    lexxos::devices::PicoCalcSouthbridgeRuntimeService& service,
	    std::uint32_t nowMs) {
		if (g_restoreKeyboardBacklightValid) {
			return g_restoreKeyboardBacklight;
		}

		(void) service.refreshSouthbridgeSnapshot(nowMs);
		lexxos::devices::PicoCalcSouthbridgeSnapshot snapshot{};
		if (service.southbridgeSnapshot(snapshot)
		    && snapshot.has(lexxos::devices::PicoCalcSouthbridgeSnapshot::validKeyboardBacklight)) {
			return snapshot.keyboardBacklight;
		}
		return 0;
	}

	bool requestLcdBacklight(
	    lexxos::devices::PicoCalcSouthbridgeRuntimeService& service,
	    std::uint8_t value,
	    std::uint32_t nowMs,
	    ApplyResult& result) {
		result.lcdBacklightAttempted = true;
		result.lcdBacklightValue = value;
		result.lcdBacklightValueValid = true;
		result.lcdBacklightOk = service.requestLcdBacklight(value, nowMs);
		auto const status = service.southbridgeLastCommandStatus();
		if (status.acceptedValueValid) {
			result.lcdBacklightValue = status.acceptedValue;
		}
		return result.lcdBacklightOk;
	}

	bool requestKeyboardBacklight(
	    lexxos::devices::PicoCalcSouthbridgeRuntimeService& service,
	    std::uint8_t value,
	    std::uint32_t nowMs,
	    ApplyResult& result) {
		result.keyboardBacklightAttempted = true;
		result.keyboardBacklightValue = value;
		result.keyboardBacklightValueValid = true;
		result.keyboardBacklightOk = service.requestKeyboardBacklight(value, nowMs);
		auto const status = service.southbridgeLastCommandStatus();
		if (status.acceptedValueValid) {
			result.keyboardBacklightValue = status.acceptedValue;
		}
		return result.keyboardBacklightOk;
	}
	#endif
#endif

} // namespace

std::string_view settingsPath() {
	return displayPowerConfigPath;
}

IdleSettings idleSettings(lexxos::platform::PlatformServices const& platform) {
#if LEXXOS_DISPLAY_POWER_POLICY_ENABLED
	loadConfigIfDue(uptimeMs(platform), true);
	return g_idleSettings;
#else
	(void) platform;
	return {};
#endif
}

bool configureIdleTimeout(
    lexxos::platform::PlatformServices const& platform,
    std::uint32_t timeoutMs,
    lexxos::devices::DisplayPowerMode mode,
    bool save,
    IdleSettings* settings) {
#if LEXXOS_DISPLAY_POWER_POLICY_ENABLED
	IdleSettings updated = g_idleSettings;
	updated.timeoutMs = timeoutMs;
	updated.mode = mode;
	updated.loadedFromConfig = false;
	if (updated.timeoutMs == 0 || updated.dimTimeoutMs >= updated.timeoutMs) {
		updated.dimTimeoutMs = 0;
	}
	if (!validateSettings(updated)) {
		if (settings != nullptr) {
			*settings = g_idleSettings;
		}
		return false;
	}
	bool const saved = !save || saveConfig(updated);
	updated.loadedFromConfig = save && saved;
	g_idleSettings = updated;
	g_configLoaded = true;
	auto const nowMs = uptimeMs(platform);
	g_lastActivityMs = nowMs;
	g_nextIdleCheckMs = nowMs + idleCheckIntervalMs;
	g_idleStage = IdleStage::Active;
	if (settings != nullptr) {
		*settings = g_idleSettings;
	}
	return saved;
#else
	(void) platform;
	(void) timeoutMs;
	(void) mode;
	(void) save;
	if (settings != nullptr) {
		*settings = {};
	}
	return false;
#endif
}

void recordActivity(lexxos::platform::PlatformServices const& platform) {
#if LEXXOS_DISPLAY_POWER_POLICY_ENABLED
	g_lastActivityMs = uptimeMs(platform);
#else
	(void) platform;
#endif
}

bool updateIdle(
    lexxos::devices::DeviceManager& devices,
    lexxos::platform::PlatformServices const& platform) {
#if LEXXOS_DISPLAY_POWER_POLICY_ENABLED
	auto const nowMs = uptimeMs(platform);
	loadConfigIfDue(nowMs, false);

	if (g_nextIdleCheckMs != 0 && !timeReached(nowMs, g_nextIdleCheckMs)) {
		return false;
	}
	g_nextIdleCheckMs = nowMs + idleCheckIntervalMs;

	if (g_idleSettings.timeoutMs == 0) {
		return false;
	}
	if (g_lastActivityMs == 0) {
		g_lastActivityMs = nowMs;
		return false;
	}

	auto* display = devices.primaryDisplayDevice();
	if (display == nullptr || !display->supportsPowerMode()) {
		return false;
	}
	if (display->powerMode() != lexxos::devices::DisplayPowerMode::On) {
		g_idleStage = IdleStage::Off;
		return false;
	}

	auto const idleMs = nowMs - g_lastActivityMs;
	if (idleMs >= g_idleSettings.timeoutMs) {
		ApplyResult result{};
		(void) applyPowerMode(
		    devices,
		    platform,
		    *display,
		    g_idleSettings.mode,
		    "display-idle",
		    lexxos::devices::DeviceResourcePriority::Background,
		    &result);
		if (result.displayOk) {
			g_idleStage = IdleStage::Off;
		}
		return result.ok();
	}

	if (g_idleSettings.dimTimeoutMs != 0
	    && g_idleStage == IdleStage::Active
	    && idleMs >= g_idleSettings.dimTimeoutMs) {
		return dimDisplayBacklight(
		    devices,
		    *display,
		    "display-idle-dim",
		    lexxos::devices::DeviceResourcePriority::Background);
	}
	return false;
#else
	(void) devices;
	(void) platform;
	return false;
#endif
}

bool applyPowerMode(
    lexxos::devices::DeviceManager& devices,
    lexxos::platform::PlatformServices const& platform,
    lexxos::devices::DisplayDevice& display,
    lexxos::devices::DisplayPowerMode mode,
    std::string_view owner,
    lexxos::devices::DeviceResourcePriority priority,
    ApplyResult* result) {
	ApplyResult localResult{};

	auto displayLock = devices.lockDisplayDevice(display.name(), priority, owner);
	localResult.displayLockOk = static_cast<bool>(displayLock);
	if (!displayLock) {
		if (result != nullptr) {
			*result = localResult;
		}
		return false;
	}

#if LEXXOS_DISPLAY_POWER_POLICY_ENABLED
	auto const nowMs = uptimeMs(platform);
	if (mode != lexxos::devices::DisplayPowerMode::On) {
		rememberDisplayBacklight(display);
	}

	#if BOARD_FAMILY_ENABLED(PICOCALC) && FEATURE_ENABLED(DISPLAY)
	auto* service = southbridgeService(devices);
	if (service != nullptr && mode != lexxos::devices::DisplayPowerMode::On) {
		auto serviceLock = lockSouthbridge(devices, owner, priority);
		if (serviceLock) {
			rememberCurrentBacklight(*service, nowMs);
		}
	}
	#endif
#else
	(void) platform;
#endif

	localResult.displayOk = display.setPowerMode(mode);

#if LEXXOS_DISPLAY_POWER_POLICY_ENABLED
	if (localResult.displayOk) {
		if (mode == lexxos::devices::DisplayPowerMode::On) {
			g_lastActivityMs = nowMs;
			g_idleStage = IdleStage::Active;
			(void) requestDisplayBacklight(display, wakeDisplayBacklightValue(display), localResult);
		} else {
			g_idleStage = IdleStage::Off;
			(void) requestDisplayBacklight(display, 0, localResult);
		}
	}

	#if BOARD_FAMILY_ENABLED(PICOCALC) && FEATURE_ENABLED(DISPLAY)
	if (localResult.displayOk && service != nullptr) {
		auto serviceLock = lockSouthbridge(devices, owner, priority);
		if (serviceLock) {
			auto const lcdValue = mode == lexxos::devices::DisplayPowerMode::On ? wakeBacklightValue(*service, nowMs) : 0;
			auto const keyboardValue = mode == lexxos::devices::DisplayPowerMode::On ? wakeKeyboardBacklightValue(*service, nowMs) : 0;
			(void) requestLcdBacklight(*service, lcdValue, nowMs, localResult);
			(void) requestKeyboardBacklight(*service, keyboardValue, nowMs, localResult);
		} else {
			localResult.lcdBacklightAttempted = true;
			localResult.lcdBacklightOk = false;
			localResult.lcdBacklightValueValid = mode != lexxos::devices::DisplayPowerMode::On;
			localResult.lcdBacklightValue = 0;
			localResult.keyboardBacklightAttempted = true;
			localResult.keyboardBacklightOk = false;
			localResult.keyboardBacklightValueValid = mode != lexxos::devices::DisplayPowerMode::On;
			localResult.keyboardBacklightValue = 0;
		}
	}
	#endif
#endif

	if (result != nullptr) {
		*result = localResult;
	}
	return localResult.ok();
}

bool wakeForInput(
    lexxos::devices::DeviceManager& devices,
    lexxos::platform::PlatformServices const& platform,
    std::string_view owner) {
#if LEXXOS_DISPLAY_POWER_POLICY_ENABLED
	auto* display = devices.primaryDisplayDevice();
	if (display == nullptr || !display->supportsPowerMode()) {
		return false;
	}
	if (display->powerMode() == lexxos::devices::DisplayPowerMode::On && g_idleStage == IdleStage::Active) {
		return false;
	}

	ApplyResult result{};
	(void) applyPowerMode(
	    devices,
	    platform,
	    *display,
	    lexxos::devices::DisplayPowerMode::On,
	    owner,
	    lexxos::devices::DeviceResourcePriority::System,
	    &result);
	return result.displayOk;
#else
	(void) devices;
	(void) platform;
	(void) owner;
	return false;
#endif
}

} // namespace lexxos::app::picocalc_display_power
