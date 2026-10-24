#include <lexxos/apps/devices/display_power_program.h>

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <lexxos/app/picocalc_display_power.h>
#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/devices/detail/display_program_support.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/feature.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::device_programs {
namespace {

	constexpr lexxos::program::ProgramArgumentSpec displayPowerArguments[] = {
	    {
         .name = "action",
         .valueName = "on|off|sleep|timeout|name",
         .help = "power mode, timeout action or display device name",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	    {
         .name = "value",
         .valueName = "name|seconds|off",
         .help = "display device name, timeout seconds or timeout off",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	    {
         .name = "extra",
         .valueName = "off|sleep",
         .help = "idle timeout display mode",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	};

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

	[[nodiscard]] bool parsePowerMode(std::string_view text, lexxos::devices::DisplayPowerMode& mode) {
		if (equalsIgnoreCase(text, "on")) {
			mode = lexxos::devices::DisplayPowerMode::On;
			return true;
		}
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

	[[nodiscard]] bool parseIdleMode(std::string_view text, lexxos::devices::DisplayPowerMode& mode) {
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

	[[nodiscard]] bool isTimeoutAction(std::string_view text) {
		return equalsIgnoreCase(text, "timeout") || equalsIgnoreCase(text, "idle") || equalsIgnoreCase(text, "auto");
	}

	[[nodiscard]] bool parseTimeoutMs(std::string_view text, std::uint32_t& timeoutMs) {
		if (equalsIgnoreCase(text, "off") || equalsIgnoreCase(text, "disable") || equalsIgnoreCase(text, "disabled")) {
			timeoutMs = 0;
			return true;
		}

		std::uint32_t seconds = 0;
		if (!parseUnsigned32(text, seconds) || seconds > 86400U) {
			return false;
		}
		timeoutMs = seconds * 1000U;
		return true;
	}

	void writeIdleSettings(
	    lexxos::os::AppContext& context,
	    lexxos::app::picocalc_display_power::IdleSettings const& settings,
	    bool includeSaveStatus = false,
	    bool saved = true) {
		context.write("display-power: dim=");
		if (settings.dimTimeoutMs == 0) {
			context.write("off");
		} else {
			writeUnsigned(context, settings.dimTimeoutMs / 1000U);
			context.write("s");
		}
		context.write(" dim-backlight=");
		writeUnsigned(context, settings.dimBacklight);
		context.write(" timeout=");
		if (settings.timeoutMs == 0) {
			context.write("off");
		} else {
			writeUnsigned(context, settings.timeoutMs / 1000U);
			context.write("s");
		}
		context.write(" mode=");
		context.write(lexxos::devices::toString(settings.mode));
		context.write(" config=");
		context.write(settings.loadedFromConfig ? "loaded" : "runtime");
		if (includeSaveStatus) {
			context.write(" save=");
			context.write(saved ? "ok" : "failed");
		}
		context.write(" path=");
		context.writeLine(lexxos::app::picocalc_display_power::settingsPath());
	}

	void writePowerMode(
	    lexxos::os::AppContext& context,
	    lexxos::devices::DisplayDevice const& display,
	    lexxos::app::picocalc_display_power::ApplyResult const* result = nullptr) {
		context.write("display-power: ");
		context.write(display.name());
		context.write(" mode=");
		if (display.supportsPowerMode()) {
			context.write(lexxos::devices::toString(display.powerMode()));
		} else {
			context.write("unsupported");
		}
		if (result != nullptr && result->lcdBacklightAttempted) {
			context.write(" lcd-backlight=");
			if (result->lcdBacklightOk && result->lcdBacklightValueValid) {
				writeUnsigned(context, result->lcdBacklightValue);
			} else if (result->lcdBacklightOk) {
				context.write("ok");
			} else {
				context.write("failed");
			}
		} else if (result == nullptr && display.supportsBacklight()) {
			context.write(" lcd-backlight=");
			writeUnsigned(context, display.backlight());
		}
		if (result != nullptr && result->keyboardBacklightAttempted) {
			context.write(" keyboard-backlight=");
			if (result->keyboardBacklightOk && result->keyboardBacklightValueValid) {
				writeUnsigned(context, result->keyboardBacklightValue);
			} else if (result->keyboardBacklightOk) {
				context.write("ok");
			} else {
				context.write("failed");
			}
		}
		context.writeLine("");
	}

} // namespace

lexxos::program::ProgramDescriptor DisplayPowerProgram::descriptor() {
	return {
	    .name = "display-power",
	    .help = "show or set display power mode",
	    .description = "Shows or switches display controller power mode. Without a name it uses the primary display.",
	    .usage = "display-power [on|off|sleep|timeout|name] [name|seconds|off] [off|sleep]",
	    .arguments = argumentList(displayPowerArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int DisplayPowerProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto const action = arguments.positional("action");
	auto const value = arguments.positional("value");
	auto const extra = arguments.positional("extra");

	if (isTimeoutAction(action)) {
#if FEATURE_ENABLED(DISPLAY)
		if (value.empty()) {
			writeIdleSettings(context, lexxos::app::picocalc_display_power::idleSettings(context.platform()));
			return 0;
		}

		std::uint32_t timeoutMs = 0;
		if (!parseTimeoutMs(value, timeoutMs)) {
			context.writeErrorLine("display-power: invalid timeout, expected off or 0..86400 seconds");
			return 2;
		}

		auto settings = lexxos::app::picocalc_display_power::idleSettings(context.platform());
		auto mode = settings.mode;
		if (!extra.empty() && !parseIdleMode(extra, mode)) {
			context.writeErrorLine("display-power: invalid timeout mode, expected off or sleep");
			return 2;
		}

		auto const saved = lexxos::app::picocalc_display_power::configureIdleTimeout(
		    context.platform(),
		    timeoutMs,
		    mode,
		    true,
		    &settings);
		writeIdleSettings(context, settings, true, saved);
		return saved ? 0 : 1;
#else
		(void) value;
		(void) extra;
		context.writeErrorLine("display-power: idle timeout is unsupported");
		return 1;
#endif
	}

	bool hasRequestedMode = false;
	lexxos::devices::DisplayPowerMode requestedMode{};
	auto deviceName = value;
	if (!action.empty()) {
		if (parsePowerMode(action, requestedMode)) {
			hasRequestedMode = true;
		} else if (value.empty()) {
			deviceName = action;
		} else {
			context.writeErrorLine("display-power: invalid action, expected on, off, sleep, timeout or display name");
			return 2;
		}
	}
	if (!extra.empty()) {
		context.writeErrorLine("display-power: too many arguments");
		return 2;
	}

	auto* display = detail::selectedDisplay(context, deviceName);
	if (display == nullptr) {
		context.writeErrorLine("display-power: display device is not registered");
		return 1;
	}

	if (!hasRequestedMode) {
		writePowerMode(context, *display);
		return 0;
	}

	if (!display->supportsPowerMode()) {
		context.writeErrorLine("display-power: display power mode is unsupported");
		return 1;
	}
	if (!display->isReady()) {
		context.writeErrorLine("display-power: display device is not ready");
		return 1;
	}

	lexxos::app::picocalc_display_power::ApplyResult result{};
	(void) lexxos::app::picocalc_display_power::applyPowerMode(
	    context.devices(),
	    context.platform(),
	    *display,
	    requestedMode,
	    "display-power",
	    lexxos::devices::DeviceResourcePriority::Interactive,
	    &result);

	if (!result.displayLockOk) {
		context.writeErrorLine("display-power: display device is busy");
		return 1;
	}
	if (!result.displayOk) {
		context.writeErrorLine("display-power: request failed");
		return 1;
	}

	writePowerMode(context, *display, &result);
	return result.ok() ? 0 : 1;
}

} // namespace lexxos::apps::device_programs
