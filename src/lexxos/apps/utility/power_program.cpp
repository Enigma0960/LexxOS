#include <lexxos/apps/utility/power_program.h>

#include <cstdint>
#include <string_view>

#include <lexxos/app/picocalc_display_power.h>
#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/feature.h>
#include <lexxos/os/app_context.h>
#include <lexxos/platform/platform_services.h>

#if FEATURE_ENABLED(PICO_CALC)
	#include <lexxos/devices/picocalc/picocalc_southbridge.h>
	#include <lexxos/devices/picocalc/picocalc_southbridge_resource_policy.h>
	#include <lexxos/devices/picocalc/picocalc_southbridge_runtime.h>
#endif

namespace lexxos::apps::utility_programs {
namespace {

#if FEATURE_ENABLED(PICO_CALC)
	lexxos::devices::PicoCalcSouthbridgeRuntimeService* picoCalcSouthbridge(lexxos::os::AppContext& context) {
		return lexxos::devices::PicoCalcSouthbridgeResourcePolicy::readyService(context.devices());
	}

	lexxos::devices::DeviceResourceLock lockPicoCalcSouthbridge(lexxos::os::AppContext& context) {
		return lexxos::devices::PicoCalcSouthbridgeResourcePolicy::lock(
		    context.devices(),
		    "power",
		    lexxos::devices::DeviceResourcePriority::Interactive);
	}

	std::uint8_t batteryPercent(lexxos::devices::PicoCalcBatteryStatus const& status) {
		auto const value = status.value();
		return value > 100 ? 100 : value;
	}

	std::string_view batteryLevelName(lexxos::devices::PicoCalcBatteryStatus const& status) {
		auto const percent = batteryPercent(status);
		if (status.raw == 0) {
			return "empty-or-disconnected";
		}
		if (percent <= 1) {
			return "critical";
		}
		if (percent <= 20) {
			return "low";
		}
		if (percent < 66) {
			return "medium";
		}
		return "good";
	}

	void writeBatteryStatus(
	    lexxos::os::AppContext& context,
	    lexxos::devices::PicoCalcBatteryStatus const& status) {
		context.write("battery: raw=");
		lexxos::apps::writeHex8(context, status.raw);
		context.write(" charging=");
		context.write(status.charging() ? "yes" : "no");
		context.write(" percent=");
		lexxos::apps::writeUnsigned(context, batteryPercent(status));
		context.write("% level=");
		context.write(batteryLevelName(status));
		context.writeLine("");
	}

	void writeByteField(
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

	std::string_view commandKindName(lexxos::devices::PicoCalcSouthbridgeCommandKind kind) {
		switch (kind) {
			case lexxos::devices::PicoCalcSouthbridgeCommandKind::None:
				return "none";
			case lexxos::devices::PicoCalcSouthbridgeCommandKind::LcdBacklight:
				return "lcd-backlight";
			case lexxos::devices::PicoCalcSouthbridgeCommandKind::KeyboardBacklight:
				return "keyboard-backlight";
			case lexxos::devices::PicoCalcSouthbridgeCommandKind::Reset:
				return "reset";
			case lexxos::devices::PicoCalcSouthbridgeCommandKind::PowerOff:
				return "poweroff";
		}
		return "unknown";
	}

	std::string_view commandStateName(lexxos::devices::PicoCalcSouthbridgeCommandState state) {
		switch (state) {
			case lexxos::devices::PicoCalcSouthbridgeCommandState::None:
				return "none";
			case lexxos::devices::PicoCalcSouthbridgeCommandState::Running:
				return "running";
			case lexxos::devices::PicoCalcSouthbridgeCommandState::Ok:
				return "ok";
			case lexxos::devices::PicoCalcSouthbridgeCommandState::Failed:
				return "failed";
			case lexxos::devices::PicoCalcSouthbridgeCommandState::Refused:
				return "refused";
		}
		return "unknown";
	}

	void writeLastCommand(lexxos::os::AppContext& context, lexxos::devices::PicoCalcSouthbridgeCommandStatus const& status) {
		context.write("last-command: ");
		context.write(commandKindName(status.kind));
		context.write(" state=");
		context.write(commandStateName(status.state));
		if (status.kind != lexxos::devices::PicoCalcSouthbridgeCommandKind::None) {
			context.write(" requested=");
			lexxos::apps::writeUnsigned(context, status.requestedValue);
			if (status.acceptedValueValid) {
				context.write(" accepted=");
				lexxos::apps::writeUnsigned(context, status.acceptedValue);
			}
		}
		context.writeLine("");
	}
#endif

	std::string_view platformBatteryLevelName(std::uint8_t percent) {
		if (percent <= 1) {
			return "critical";
		}
		if (percent <= 20) {
			return "low";
		}
		if (percent < 66) {
			return "medium";
		}
		return "good";
	}

	void writePlatformBatteryStatus(
	    lexxos::os::AppContext& context,
	    lexxos::platform::PlatformBatteryStatus const& status) {
		context.write("battery:");
		if (status.voltageMvValid) {
			context.write(" voltage=");
			lexxos::apps::writeUnsigned(context, status.voltageMv);
			context.write("mV");
		}
		if (status.percentValid) {
			context.write(" percent=");
			lexxos::apps::writeUnsigned(context, status.percent);
			context.write("% level=");
			context.write(platformBatteryLevelName(status.percent));
		}
		context.write(" charging=");
		if (status.chargingValid) {
			context.write(status.charging ? "yes" : "no");
		} else {
			context.write("unavailable");
		}
		context.writeLine("");
	}

#if FEATURE_ENABLED(DISPLAY)
	void writeDisplayIdleSettings(lexxos::os::AppContext& context) {
		auto const settings = lexxos::app::picocalc_display_power::idleSettings(context.platform());
		context.write("display-idle: dim=");
		if (settings.dimTimeoutMs == 0) {
			context.write("off");
		} else {
			lexxos::apps::writeUnsigned(context, settings.dimTimeoutMs / 1000U);
			context.write("s");
		}
		context.write(" dim-backlight=");
		lexxos::apps::writeUnsigned(context, settings.dimBacklight);
		context.write(" timeout=");
		if (settings.timeoutMs == 0) {
			context.write("off");
		} else {
			lexxos::apps::writeUnsigned(context, settings.timeoutMs / 1000U);
			context.write("s");
		}
		context.write(" mode=");
		context.write(lexxos::devices::toString(settings.mode));
		context.write(" config=");
		context.write(settings.loadedFromConfig ? "loaded" : "runtime");
		context.write(" path=");
		context.writeLine(lexxos::app::picocalc_display_power::settingsPath());
	}

	bool writeDisplayPowerStatus(lexxos::os::AppContext& context) {
		auto* display = context.devices().primaryDisplayDevice();
		if (display == nullptr) {
			return false;
		}
		context.write("display: ");
		context.write(display->name());
		context.write(" ready=");
		context.write(display->isReady() ? "yes" : "no");
		context.write(" power-mode=");
		if (display->supportsPowerMode()) {
			context.write(lexxos::devices::toString(display->powerMode()));
		} else {
			context.write("unsupported");
		}
		context.write(" backlight=");
		if (display->supportsBacklight()) {
			lexxos::apps::writeUnsigned(context, display->backlight());
		} else {
			context.write("unsupported");
		}
		context.writeLine("");
		writeDisplayIdleSettings(context);
		return true;
	}
#endif

} // namespace

lexxos::program::ProgramDescriptor PowerProgram::descriptor() {
	return {
	    .name = "power",
	    .help = "show board power status",
	    .description = "Shows board power, battery and power-control state when supported.",
	    .usage = "power",
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int PowerProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
#if FEATURE_ENABLED(PICO_CALC)
	auto* service = picoCalcSouthbridge(context);
	if (service == nullptr) {
		context.writeErrorLine("power: board power service unavailable");
		return 1;
	}

	auto serviceLock = lockPicoCalcSouthbridge(context);
	if (!serviceLock) {
		context.writeErrorLine("power: southbridge service is busy");
		return 1;
	}

	bool const refreshed = service->refreshSouthbridgeSnapshot(lexxos::apps::uptimeMs(context));
	(void) refreshed;
	lexxos::devices::PicoCalcSouthbridgeSnapshot snapshot{};
	if (!service->southbridgeSnapshot(snapshot)) {
		context.writeErrorLine("power: no southbridge snapshot available");
		return 1;
	}

	context.writeLine("power: source=picocalc-southbridge direct-i2c=no");
	if (snapshot.has(lexxos::devices::PicoCalcSouthbridgeSnapshot::validBattery)) {
		writeBatteryStatus(context, snapshot.battery);
		context.writeLine("low-battery-policy: southbridge-owned; shell remains active while snapshot is readable");
	} else {
		context.writeLine("battery: unavailable");
		context.writeLine("low-battery-policy: unavailable until battery snapshot is readable");
	}

	context.write("power-off-supported: ");
	if (snapshot.has(lexxos::devices::PicoCalcSouthbridgeSnapshot::validPowerOffSupport)) {
		context.write(snapshot.powerOffSupport > 0 ? "yes (" : "no (");
		lexxos::apps::writeHex8(context, snapshot.powerOffSupport);
		context.writeLine(")");
	} else {
		context.writeLine("unavailable");
	}
	context.writeLine("power-key: not-keyboard-fifo");
	context.writeLine("keyboard-backlight-write: use backlight kbd <0..255>; firmware may quantize accepted values");
	writeByteField(
	    context,
	    "lcd-backlight",
	    snapshot.has(lexxos::devices::PicoCalcSouthbridgeSnapshot::validLcdBacklight),
	    snapshot.lcdBacklight);
	writeByteField(
	    context,
	    "keyboard-backlight",
	    snapshot.has(lexxos::devices::PicoCalcSouthbridgeSnapshot::validKeyboardBacklight),
	    snapshot.keyboardBacklight);
	writeLastCommand(context, service->southbridgeLastCommandStatus());
	return 0;
#else
	auto const& power = context.platform().power;
	bool wroteBattery = false;
	lexxos::platform::PlatformBatteryStatus status{};
	if (power.readBattery != nullptr && power.readBattery(status) && status.available) {
		context.write("power: source=");
		context.write(status.source != nullptr ? status.source : "platform");
		context.writeLine(" direct-i2c=no");
		writePlatformBatteryStatus(context, status);
		wroteBattery = true;
	} else {
		context.writeLine("power: source=platform direct-i2c=no");
		context.writeLine("battery: unavailable");
	}

	#if FEATURE_ENABLED(DISPLAY)
	bool const wroteDisplay = writeDisplayPowerStatus(context);
	#else
	bool const wroteDisplay = false;
	#endif
	#if FEATURE_ENABLED(DISPLAY)
	context.writeLine(wroteDisplay ? "power-off-supported: software-halt display-off" : "power-off-supported: no");
	#else
	context.writeLine("power-off-supported: no");
	#endif
	context.writeLine("charging-current: unavailable");
	context.writeLine(wroteBattery ? "low-battery-policy: platform voltage estimate only" : "low-battery-policy: unavailable");
	return (wroteBattery || wroteDisplay) ? 0 : 1;
#endif
}

} // namespace lexxos::apps::utility_programs
