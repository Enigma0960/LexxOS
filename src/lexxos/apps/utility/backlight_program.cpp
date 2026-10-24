#include <lexxos/apps/utility/backlight_program.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/utility/detail/backlight_settings.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/feature.h>
#include <lexxos/os/app_context.h>
#include <lexxos/vfs/vfs.h>

#if FEATURE_ENABLED(PICO_CALC)
	#include <lexxos/devices/picocalc/picocalc_southbridge.h>
	#include <lexxos/devices/picocalc/picocalc_southbridge_resource_policy.h>
	#include <lexxos/devices/picocalc/picocalc_southbridge_runtime.h>
#endif

namespace lexxos::apps::utility_programs {
namespace {

	using detail::BacklightSelection;
	using detail::BacklightSettings;

	constexpr lexxos::program::ProgramArgumentSpec backlightArguments[] = {
	    {
         .name = "target",
         .valueName = "lcd|kbd|save|restore",
         .help = "backlight target or action",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	    {
         .name = "value",
         .valueName = "0..255|lcd|kbd|all",
         .help = "brightness or save/restore target",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	};

	constexpr std::string_view backlightConfigDirectory = "/mnt/sd/.lexx";
	constexpr std::string_view backlightConfigSubdirectory = "";
	constexpr std::string_view backlightConfigPath = "/mnt/sd/.lexx/backlight.conf";
#if FEATURE_ENABLED(PICO_CALC)
	constexpr std::string_view legacyBacklightConfigPath = "/mnt/sd/lexxos/picocalc/backlight.conf";
#endif

	bool isLcdTarget(std::string_view value) {
		return value == "lcd" || value == "display" || value == "screen";
	}

	bool isKeyboardTarget(std::string_view value) {
		return value == "kbd" || value == "keyboard" || value == "keys";
	}

	bool isSaveAction(std::string_view value) {
		return value == "save";
	}

	bool isRestoreAction(std::string_view value) {
		return value == "restore" || value == "load";
	}

	[[maybe_unused]] BacklightSelection parseSelection(std::string_view value, BacklightSelection fallback) {
		if (value.empty() || value == "all") {
			return fallback;
		}
		if (isLcdTarget(value)) {
			return BacklightSelection::Lcd;
		}
		if (isKeyboardTarget(value)) {
			return BacklightSelection::Keyboard;
		}
		return BacklightSelection::Invalid;
	}

	bool parseByte(std::string_view text, std::uint8_t& value) {
		std::uint32_t parsed = 0;
		if (!lexxos::apps::parseUnsigned32(text, parsed) || parsed > 255U) {
			return false;
		}
		value = static_cast<std::uint8_t>(parsed);
		return true;
	}

	std::string_view trim(std::string_view value) {
		while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r')) {
			value.remove_prefix(1);
		}
		while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) {
			value.remove_suffix(1);
		}
		return value;
	}

	bool appendChar(std::array<char, 96>& buffer, std::size_t& offset, char value) {
		if (offset >= buffer.size()) {
			return false;
		}
		buffer[offset++] = value;
		return true;
	}

	bool appendText(std::array<char, 96>& buffer, std::size_t& offset, std::string_view text) {
		if (text.size() > buffer.size() - offset) {
			return false;
		}
		for (char const character : text) {
			buffer[offset++] = character;
		}
		return true;
	}

	bool appendUnsigned(std::array<char, 96>& buffer, std::size_t& offset, std::uint8_t value) {
		char digits[3]{};
		std::size_t digitCount = 0;
		auto remaining = value;
		do {
			digits[digitCount++] = static_cast<char>('0' + remaining % 10U);
			remaining = static_cast<std::uint8_t>(remaining / 10U);
		} while (remaining != 0);

		while (digitCount > 0) {
			if (!appendChar(buffer, offset, digits[--digitCount])) {
				return false;
			}
		}
		return true;
	}

	bool makeConfigText(BacklightSettings const& settings, std::array<char, 96>& buffer, std::size_t& size) {
		size = 0;
		if (settings.hasLcd
		    && (!appendText(buffer, size, "lcd=")
		        || !appendUnsigned(buffer, size, settings.lcd)
		        || !appendChar(buffer, size, '\n'))) {
			return false;
		}
		if (settings.hasKeyboard
		    && (!appendText(buffer, size, "keyboard=")
		        || !appendUnsigned(buffer, size, settings.keyboard)
		        || !appendChar(buffer, size, '\n'))) {
			return false;
		}
		return settings.hasLcd || settings.hasKeyboard;
	}

	bool parseConfigText(std::string_view text, BacklightSettings& settings) {
		settings = {};
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
			std::uint8_t value = 0;
			if (!parseByte(valueText, value)) {
				return false;
			}

			if (isLcdTarget(name)) {
				settings.lcd = value;
				settings.hasLcd = true;
			} else if (isKeyboardTarget(name)) {
				settings.keyboard = value;
				settings.hasKeyboard = true;
			} else {
				return false;
			}
		}
		return settings.hasLcd || settings.hasKeyboard;
	}

	bool readConfigFromPath(std::string_view path, BacklightSettings& settings) {
		lexxos::vfs::FileHandle file;
		auto const openStatus = lexxos::vfs::open(path, lexxos::vfs::OpenMode::Read, file);
		if (openStatus != lexxos::vfs::OpenStatus::Ok) {
			return false;
		}

		std::array<char, 96> buffer{};
		std::size_t bytesRead = 0;
		auto const readStatus = file.read(buffer.data(), buffer.size(), bytesRead);
		if (readStatus != lexxos::vfs::ReadStatus::Ok) {
			return false;
		}
		return parseConfigText(std::string_view{buffer.data(), bytesRead}, settings);
	}

	[[maybe_unused]] bool readConfig(BacklightSettings& settings) {
		if (readConfigFromPath(backlightConfigPath, settings)) {
			return true;
		}
#if FEATURE_ENABLED(PICO_CALC)
		return readConfigFromPath(legacyBacklightConfigPath, settings);
#else
		return false;
#endif
	}

	bool ensureDirectory(std::string_view path) {
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

	[[maybe_unused]] lexxos::vfs::WriteStatus saveConfig(BacklightSettings const& settings) {
		if (!ensureDirectory(backlightConfigDirectory) || !ensureDirectory(backlightConfigSubdirectory)) {
			return lexxos::vfs::WriteStatus::NotFound;
		}

		std::array<char, 96> buffer{};
		std::size_t size = 0;
		if (!makeConfigText(settings, buffer, size)) {
			return lexxos::vfs::WriteStatus::TooLarge;
		}

		auto const writeStatus = lexxos::vfs::writeText(
		    backlightConfigPath,
		    std::string_view{buffer.data(), size},
		    lexxos::vfs::WriteMode::CreateTruncate);
		if (writeStatus != lexxos::vfs::WriteStatus::Ok) {
			return writeStatus;
		}
		return lexxos::vfs::sync() == lexxos::vfs::SyncStatus::Ok
		    ? lexxos::vfs::WriteStatus::Ok
		    : lexxos::vfs::WriteStatus::NotWritable;
	}

	[[maybe_unused]] void writeSnapshotBacklight(
	    lexxos::os::AppContext& context,
	    std::string_view name,
	    bool hasValue,
	    std::uint8_t value) {
		context.write(name);
		context.write(": ");
		if (!hasValue) {
			context.writeLine("unavailable");
			return;
		}
		lexxos::apps::writeUnsigned(context, value);
		context.write(" (");
		lexxos::apps::writeHex8(context, value);
		context.writeLine(")");
	}

	[[maybe_unused]] void writeSavedSettings(lexxos::os::AppContext& context, std::string_view prefix, BacklightSettings const& settings) {
		context.write(prefix);
		if (settings.hasLcd) {
			context.write(" lcd=");
			lexxos::apps::writeUnsigned(context, settings.lcd);
		}
		if (settings.hasKeyboard) {
			context.write(" keyboard=");
			lexxos::apps::writeUnsigned(context, settings.keyboard);
		}
		context.write(" path=");
		context.writeLine(backlightConfigPath);
	}

	[[maybe_unused]] void writeRestoreResult(
	    lexxos::os::AppContext& context,
	    BacklightSettings const& restored,
	    BacklightSettings const& failed,
	    bool anyOk) {
		context.write(anyOk ? "backlight: restored" : "backlight: restore failed");
		if (restored.hasLcd) {
			context.write(" lcd=");
			lexxos::apps::writeUnsigned(context, restored.lcd);
		}
		if (failed.hasLcd) {
			context.write(" lcd=failed");
		}
		if (restored.hasKeyboard) {
			context.write(" keyboard=");
			lexxos::apps::writeUnsigned(context, restored.keyboard);
		}
		if (failed.hasKeyboard) {
			context.write(" keyboard=failed");
		}
		context.write(" path=");
		context.writeLine(backlightConfigPath);
	}

#if FEATURE_ENABLED(PICO_CALC)
	lexxos::devices::PicoCalcSouthbridgeRuntimeService* picoCalcSouthbridge(lexxos::os::AppContext& context) {
		return lexxos::devices::PicoCalcSouthbridgeResourcePolicy::readyService(context.devices());
	}

	lexxos::devices::DeviceResourceLock lockPicoCalcSouthbridge(lexxos::os::AppContext& context) {
		return lexxos::devices::PicoCalcSouthbridgeResourcePolicy::lock(
		    context.devices(),
		    "backlight",
		    lexxos::devices::DeviceResourcePriority::Interactive);
	}

	int showPicoCalcBacklight(
	    lexxos::os::AppContext& context,
	    lexxos::devices::PicoCalcSouthbridgeRuntimeService& service) {
		auto serviceLock = lockPicoCalcSouthbridge(context);
		if (!serviceLock) {
			context.writeErrorLine("backlight: southbridge service is busy");
			return 1;
		}

		bool const refreshed = service.refreshSouthbridgeSnapshot(lexxos::apps::uptimeMs(context));
		(void) refreshed;
		lexxos::devices::PicoCalcSouthbridgeSnapshot snapshot{};
		if (!service.southbridgeSnapshot(snapshot)) {
			context.writeErrorLine("backlight: no southbridge snapshot available");
			return 1;
		}

		context.writeLine("backlight: source=picocalc-southbridge direct-i2c=no");
		writeSnapshotBacklight(
		    context,
		    "lcd",
		    snapshot.has(lexxos::devices::PicoCalcSouthbridgeSnapshot::validLcdBacklight),
		    snapshot.lcdBacklight);
		writeSnapshotBacklight(
		    context,
		    "keyboard",
		    snapshot.has(lexxos::devices::PicoCalcSouthbridgeSnapshot::validKeyboardBacklight),
		    snapshot.keyboardBacklight);
		return 0;
	}

	int savePicoCalcBacklight(
	    lexxos::os::AppContext& context,
	    lexxos::devices::PicoCalcSouthbridgeRuntimeService& service,
	    BacklightSelection selection) {
		auto serviceLock = lockPicoCalcSouthbridge(context);
		if (!serviceLock) {
			context.writeErrorLine("backlight: southbridge service is busy");
			return 1;
		}

		bool const refreshed = service.refreshSouthbridgeSnapshot(lexxos::apps::uptimeMs(context));
		(void) refreshed;
		lexxos::devices::PicoCalcSouthbridgeSnapshot snapshot{};
		if (!service.southbridgeSnapshot(snapshot)) {
			context.writeErrorLine("backlight: current backlight levels unavailable");
			return 1;
		}

		BacklightSettings settings{};
		if ((selection == BacklightSelection::All || selection == BacklightSelection::Lcd)
		    && snapshot.has(lexxos::devices::PicoCalcSouthbridgeSnapshot::validLcdBacklight)) {
			settings.lcd = snapshot.lcdBacklight;
			settings.hasLcd = true;
		}
		if ((selection == BacklightSelection::All || selection == BacklightSelection::Keyboard)
		    && snapshot.has(lexxos::devices::PicoCalcSouthbridgeSnapshot::validKeyboardBacklight)) {
			settings.keyboard = snapshot.keyboardBacklight;
			settings.hasKeyboard = true;
		}
		if (!settings.hasLcd && !settings.hasKeyboard) {
			context.writeErrorLine("backlight: selected backlight level unavailable");
			return 1;
		}

		serviceLock.release();

		auto const status = saveConfig(settings);
		if (status != lexxos::vfs::WriteStatus::Ok) {
			context.writeError("backlight: save failed: ");
			context.writeErrorLine(lexxos::vfs::toString(status));
			return 1;
		}

		writeSavedSettings(context, "backlight: saved", settings);
		return 0;
	}

	int restorePicoCalcBacklight(
	    lexxos::os::AppContext& context,
	    lexxos::devices::PicoCalcSouthbridgeRuntimeService& service,
	    BacklightSelection selection) {
		BacklightSettings settings{};
		if (!readConfig(settings)) {
			context.writeError("backlight: cannot read config: ");
			context.writeErrorLine(backlightConfigPath);
			return 1;
		}
		if (selection == BacklightSelection::Lcd) {
			settings.hasKeyboard = false;
		} else if (selection == BacklightSelection::Keyboard) {
			settings.hasLcd = false;
		}
		if (!settings.hasLcd && !settings.hasKeyboard) {
			context.writeErrorLine("backlight: selected saved level is unavailable");
			return 1;
		}

		auto serviceLock = lockPicoCalcSouthbridge(context);
		if (!serviceLock) {
			context.writeErrorLine("backlight: southbridge service is busy");
			return 1;
		}

		auto const nowMs = lexxos::apps::uptimeMs(context);
		BacklightSettings restored{};
		BacklightSettings failed{};
		if (settings.hasLcd && !service.requestLcdBacklight(settings.lcd, nowMs)) {
			failed.lcd = settings.lcd;
			failed.hasLcd = true;
		} else if (settings.hasLcd) {
			restored.lcd = settings.lcd;
			restored.hasLcd = true;
		}
		if (settings.hasKeyboard && !service.requestKeyboardBacklight(settings.keyboard, nowMs)) {
			failed.keyboard = settings.keyboard;
			failed.hasKeyboard = true;
		} else if (settings.hasKeyboard) {
			restored.keyboard = settings.keyboard;
			restored.hasKeyboard = true;
		}

		bool const anyOk = restored.hasLcd || restored.hasKeyboard;
		writeRestoreResult(context, restored, failed, anyOk);
		return anyOk ? 0 : 1;
	}
#endif

} // namespace

lexxos::program::ProgramDescriptor BacklightProgram::descriptor() {
	return {
	    .name = "backlight",
	    .help = "show or set board backlight",
	    .description = "Shows or changes display/keyboard backlight through the board power service.",
	    .usage = "backlight [lcd|kbd <0..255>|save [lcd|kbd|all]|restore [lcd|kbd|all]|load [lcd|kbd|all]]",
	    .arguments = lexxos::apps::argumentList(backlightArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int BacklightProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
#if FEATURE_ENABLED(PICO_CALC)
	auto* service = picoCalcSouthbridge(context);
	if (service == nullptr) {
		context.writeErrorLine("backlight: board backlight service unavailable");
		return 1;
	}

	auto const target = arguments.positionalOr("target", "");
	auto const valueText = arguments.positionalOr("value", "");
	if (target.empty()) {
		return showPicoCalcBacklight(context, *service);
	}
	if (isSaveAction(target)) {
		auto const selection = parseSelection(valueText, BacklightSelection::All);
		if (selection == BacklightSelection::Invalid) {
			context.writeErrorLine("backlight: invalid save target, expected lcd, kbd or all");
			return 2;
		}
		return savePicoCalcBacklight(context, *service, selection);
	}
	if (isRestoreAction(target)) {
		auto const selection = parseSelection(valueText, BacklightSelection::All);
		if (selection == BacklightSelection::Invalid) {
			context.writeErrorLine("backlight: invalid restore target, expected lcd, kbd or all");
			return 2;
		}
		return restorePicoCalcBacklight(context, *service, selection);
	}
	if (valueText.empty()) {
		context.writeErrorLine("backlight: missing brightness value");
		return 2;
	}

	std::uint8_t value = 0;
	if (!parseByte(valueText, value)) {
		context.writeErrorLine("backlight: invalid brightness, expected 0..255");
		return 2;
	}

	bool ok = false;
	auto serviceLock = lockPicoCalcSouthbridge(context);
	if (!serviceLock) {
		context.writeErrorLine("backlight: southbridge service is busy");
		return 1;
	}

	if (isLcdTarget(target)) {
		ok = service->requestLcdBacklight(value, lexxos::apps::uptimeMs(context));
	} else if (isKeyboardTarget(target)) {
		ok = service->requestKeyboardBacklight(value, lexxos::apps::uptimeMs(context));
	} else {
		context.writeErrorLine("backlight: unknown target, expected lcd or kbd");
		return 2;
	}

	auto const status = service->southbridgeLastCommandStatus();
	if (!ok) {
		context.writeError("backlight: ");
		context.writeError(isLcdTarget(target) ? "lcd" : "keyboard");
		context.writeErrorLine(" request failed");
		return 1;
	}

	context.write("backlight: ");
	context.write(isLcdTarget(target) ? "lcd" : "keyboard");
	context.write("=");
	lexxos::apps::writeUnsigned(context, status.acceptedValueValid ? status.acceptedValue : value);
	context.writeLine("");
	return 0;
#else
	auto* display = context.devices().primaryDisplayDevice();
	if (display == nullptr || !display->supportsBacklight()) {
		context.writeErrorLine("backlight: board backlight service unavailable");
		return 1;
	}

	auto const target = arguments.positionalOr("target", "");
	auto const valueText = arguments.positionalOr("value", "");
	if (target.empty()) {
		context.writeLine("backlight: source=display-device direct-i2c=no");
		context.write("lcd: ");
		lexxos::apps::writeUnsigned(context, display->backlight());
		context.writeLine("");
		return 0;
	}
	if (isSaveAction(target) || isRestoreAction(target)) {
		context.writeErrorLine("backlight: save/restore is unsupported for this board");
		return 1;
	}
	if (!isLcdTarget(target)) {
		context.writeErrorLine("backlight: unknown target, expected lcd");
		return 2;
	}
	if (valueText.empty()) {
		context.writeErrorLine("backlight: missing brightness value");
		return 2;
	}

	std::uint8_t value = 0;
	if (!parseByte(valueText, value)) {
		context.writeErrorLine("backlight: invalid brightness, expected 0..255");
		return 2;
	}

	auto displayLock = context.devices().lockDisplayDevice(
	    display->name(),
	    lexxos::devices::DeviceResourcePriority::Interactive,
	    "backlight");
	if (!displayLock) {
		context.writeErrorLine("backlight: display device is busy");
		return 1;
	}
	if (!display->setBacklight(value)) {
		context.writeErrorLine("backlight: lcd request failed");
		return 1;
	}

	context.write("backlight: lcd=");
	lexxos::apps::writeUnsigned(context, display->backlight());
	context.writeLine("");
	return 0;
#endif
}

} // namespace lexxos::apps::utility_programs
