#include <lexxos/apps/devices/touch_calibrate_program.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <commands/command_selection.h>

#include <lexxos/app/picocalc_display_power.h>
#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/devices/detail/display_program_support.h>
#include <lexxos/apps/devices/detail/touch_program_support.h>
#include <lexxos/apps/devices/touch_programs.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/devices/pointing_device.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/app_context.h>
#include <lexxos/runtime/capabilities.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::device_programs {

using detail::touch::CalibrationSample;
using detail::touch::calibrationTargetCount;
using detail::touch::captureCalibrationSample;
using detail::touch::computeCalibration;
using detail::touch::defaultCalibrationSamples;
using detail::touch::defaultCalibrationTimeoutSeconds;
using detail::touch::drawCalibrationTarget;
using detail::touch::parseActionAndName;
using detail::touch::restoreCalibration;
using detail::touch::saveCalibrationConfig;
using detail::touch::selectTouch;
using detail::touch::showCalibration;
using detail::touch::targetMargin;
using detail::touch::writeCalibration;

#if LEXXOS_COMMAND_TOUCH_CALIBRATE

namespace {

	constexpr lexxos::program::ProgramArgumentSpec touchCalibrateArguments[] = {
	    {
	     .name = "samples",
	     .shortName = 'n',
	     .valueName = "count",
	     .help = "samples averaged at each calibration target",
	     .kind = lexxos::program::ProgramArgumentKind::Option,
	     .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
	     .required = false,
	     .defaultValue = "8",
	     },
	    {
	     .name = "timeout",
	     .shortName = 't',
	     .valueName = "seconds",
	     .help = "timeout for each target",
	     .kind = lexxos::program::ProgramArgumentKind::Option,
	     .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
	     .required = false,
	     .defaultValue = "20",
	     },
	    {
	     .name = "display",
	     .shortName = 'd',
	     .valueName = "name",
	     .help = "display device name",
	     .kind = lexxos::program::ProgramArgumentKind::Option,
	     .valueType = lexxos::program::ProgramArgumentValueType::String,
	     .required = false,
	     },
	    {
	     .name = "no-save",
	     .help = "apply calibration without saving it to SD",
	     .kind = lexxos::program::ProgramArgumentKind::Flag,
	     .required = false,
	     },
	    {
	     .name = "action",
	     .valueName = "calibrate|restore|show",
	     .help = "calibration action",
	     .kind = lexxos::program::ProgramArgumentKind::Positional,
	     .valueType = lexxos::program::ProgramArgumentValueType::String,
	     .required = false,
	     },
	    {
	     .name = "name",
	     .valueName = "name",
	     .help = "touch-capable pointing device name",
	     .kind = lexxos::program::ProgramArgumentKind::Positional,
	     .valueType = lexxos::program::ProgramArgumentValueType::String,
	     .required = false,
	     },
	};

} // namespace

lexxos::program::ProgramDescriptor TouchCalibrateProgram::descriptor() {
	return {
	    .name = "touch-calibrate",
	    .help = "calibrate touch panel",
	    .description = "Captures screen targets, applies touch orientation/range calibration and saves it to SD.",
	    .usage = "touch-calibrate [restore|show|calibrate] [name] [-d display] [-n samples] [-t seconds] [--no-save]",
	    .arguments = argumentList(touchCalibrateArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int TouchCalibrateProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	std::string_view action{};
	std::string_view name{};
	if (!parseActionAndName(arguments, action, name)) {
		context.writeErrorLine("touch-calibrate: invalid arguments");
		return 2;
	}

	if (action == "restore") {
		return restoreCalibration(context, name, "touch-calibrate");
	}

	if (action == "show") {
		return showCalibration(context, name, "touch-calibrate");
	}

	std::uint32_t samples = defaultCalibrationSamples;
	std::uint32_t timeoutSeconds = defaultCalibrationTimeoutSeconds;
	if (!parseUnsigned32(arguments.valueOr("samples", "8"), samples) || samples == 0 || samples > 64) {
		context.writeErrorLine("touch-calibrate: invalid samples value");
		return 2;
	}

	if (!parseUnsigned32(arguments.valueOr("timeout", "20"), timeoutSeconds) || timeoutSeconds == 0 || timeoutSeconds > 300) {
		context.writeErrorLine("touch-calibrate: invalid timeout value");
		return 2;
	}

	auto* touch = selectTouch(context, name, "touch-calibrate");
	if (touch == nullptr) {
		return 1;
	}

	auto* display = detail::selectedDisplay(context, arguments.valueOr("display", ""));
	if (display == nullptr || !display->isReady()) {
		context.writeErrorLine(display == nullptr
		        ? "touch-calibrate: display device is not registered"
		        : "touch-calibrate: display device is not ready");
		return 1;
	}

	auto displayLock = detail::lockDisplay(context, *display, "touch-calibrate");
	if (!displayLock) {
		context.writeErrorLine("touch-calibrate: display device is busy");
		return 1;
	}

	auto touchLock = context.devices().lockPointingDevice(touch->name(), lexxos::devices::DeviceResourcePriority::Interactive, "touch-calibrate");
	if (!touchLock) {
		context.writeErrorLine("touch-calibrate: touch device is busy");
		return 1;
	}

	std::array<CalibrationSample, calibrationTargetCount> captured{};
	captured[0].targetX = targetMargin;
	captured[0].targetY = targetMargin;
	captured[1].targetX = display->width() - targetMargin - 1;
	captured[1].targetY = targetMargin;
	captured[2].targetX = display->width() - targetMargin - 1;
	captured[2].targetY = display->height() - targetMargin - 1;
	captured[3].targetX = targetMargin;
	captured[3].targetY = display->height() - targetMargin - 1;
	captured[4].targetX = display->width() / 2;
	captured[4].targetY = display->height() / 2;

	bool ok = true;
	{
		auto graphicsMode = detail::enterGraphicsMode();
		(void) graphicsMode;
		for (std::size_t index = 0; index < captured.size(); ++index) {
			drawCalibrationTarget(*display, captured[index].targetX, captured[index].targetY);
			if (!captureCalibrationSample(context, *touch, samples, timeoutSeconds * 1000U, captured[index])) {
				ok = false;
				break;
			}
		}

		if (ok) {
			(void) display->clear(detail::rgb565(10, 40, 26));
		}
	}
	if (!ok) {
		context.writeErrorLine(context.cancellationRequested()
		        ? "touch-calibrate: canceled"
		        : "touch-calibrate: target timeout or touch was not released");
		return context.cancellationRequested() ? 130 : 1;
	}

	lexxos::devices::TouchCalibration calibration{};
	if (!computeCalibration(captured, captured.size(), display->width(), display->height(), calibration)) {
		context.writeErrorLine("touch-calibrate: failed to compute calibration");
		return 1;
	}

	if (!touch->applyTouchCalibration(calibration)) {
		context.writeErrorLine("touch-calibrate: failed to apply calibration");
		return 1;
	}

	context.writeLine("touch-calibrate: applied");
	writeCalibration(context, calibration);
	if (!arguments.has("no-save")) {
		auto const status = saveCalibrationConfig(calibration);
		if (status != lexxos::vfs::WriteStatus::Ok) {
			context.writeError("touch-calibrate: save failed: ");
			context.writeErrorLine(lexxos::vfs::toString(status));
			return 1;
		}

		context.write("touch-calibrate: saved path=");
		context.writeLine(touchCalibrationConfigPath);
	}

	return 0;
}

#endif

} // namespace lexxos::apps::device_programs
