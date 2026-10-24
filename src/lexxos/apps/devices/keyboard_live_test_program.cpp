#include <lexxos/apps/devices/keyboard_live_test_program.h>

#include <cstdint>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/devices/detail/input_program_support.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/input_device.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::device_programs {
namespace {

	constexpr lexxos::program::ProgramArgumentSpec keyboardLiveArguments[] = {
	    {
         .name = "seconds",
         .shortName = 's',
         .valueName = "seconds",
         .help = "test duration in seconds",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "30",
	     },
	    {
         .name = "interval",
         .shortName = 'i',
         .valueName = "milliseconds",
         .help = "poll interval in milliseconds",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "25",
	     },
	    {
         .name = "full",
         .shortName = 'f',
         .help = "also report unmapped raw events",
         .kind = lexxos::program::ProgramArgumentKind::Flag,
         .required = false,
	     },
	    {
         .name = "name",
         .valueName = "name",
         .help = "input device name",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	};

} // namespace

lexxos::program::ProgramDescriptor KeyboardLiveTestProgram::descriptor() {
	return {
	    .name = "keyboard-live",
	    .help = "run a live keyboard input test",
	    .description = "Polls an input device for a limited time and prints key events and pressed key changes.",
	    .usage = "keyboard-live [-s seconds] [-i milliseconds] [-f|--full] [name]",
	    .arguments = argumentList(keyboardLiveArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int KeyboardLiveTestProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	// Select a ready input device and validate timing options.
	auto const deviceName = arguments.positional("name");
	auto* input = !deviceName.empty() ? context.devices().inputDevice(deviceName) : context.devices().primaryInputDevice();
	if (input == nullptr) {
		context.writeErrorLine("keyboard-live: input device is not registered");
		return 1;
	}
	if (!input->isReady()) {
		context.writeErrorLine("keyboard-live: input device is not ready");
		return 1;
	}

	auto inputLock = detail::lockInputDeviceForInteractiveProgram(context, *input, "keyboard-live");
	if (!inputLock) {
		context.writeErrorLine("keyboard-live: input device is busy");
		return 1;
	}

	std::uint32_t seconds = 30;
	std::uint32_t intervalMs = 25;
	if (!detail::parseUnsigned(arguments.valueOr("seconds", "30"), seconds) || seconds == 0 || seconds > 3600) {
		context.writeErrorLine("keyboard-live: invalid seconds value");
		return 1;
	}
	if (!detail::parseUnsigned(arguments.valueOr("interval", "25"), intervalMs) || intervalMs == 0 || intervalMs > 1000) {
		context.writeErrorLine("keyboard-live: invalid interval value");
		return 1;
	}

	auto const fullOutput = arguments.has("full");
	input->setRawEventReporting(fullOutput);

	// Print the test configuration and initial keyboard state.
	context.write("keyboard-live: device=");
	context.write(input->name());
	context.write(" duration=");
	writeUnsigned(context, seconds);
	context.write("s interval=");
	writeUnsigned(context, intervalMs);
	context.write("ms full=");
	context.write(fullOutput ? "yes" : "no");
	context.writeLine("");
	context.writeLine("press and release keys now; the command returns automatically");

	input->update();
	auto previousMask = detail::pressedKeyMask(*input);
	auto previousRaw = input->rawState();
	detail::writePressedKeys(context, previousMask);
	detail::writeRawKeyboardState(context, *input);

	// Poll events and state changes until the requested duration expires.
	auto const startedMs = uptimeMs(context);
	auto lastHeartbeatMs = startedMs;
	auto totalEvents = std::uint32_t{0};
	auto totalChanges = std::uint32_t{0};
	auto const durationMs = seconds * 1000U;

	while (uptimeMs(context) - startedMs < durationMs) {
		if (context.pollCancellationRequest()) {
			input->setRawEventReporting(false);
			context.writeLine("keyboard-live: canceled");
			return 130;
		}

		input->update();
		totalEvents += detail::drainInputEvents(context, *input);

		auto const raw = input->rawState();
		if (raw != previousRaw) {
			previousRaw = raw;
			detail::writeRawKeyboardState(context, *input);
		}

		auto const mask = detail::pressedKeyMask(*input);
		if (mask != previousMask) {
			previousMask = mask;
			detail::writePressedKeys(context, mask);
			++totalChanges;
		}

		auto const nowMs = uptimeMs(context);
		if (nowMs - lastHeartbeatMs >= 2000U) {
			lastHeartbeatMs = nowMs;
			context.write("live: ");
			writeUnsigned(context, (nowMs - startedMs) / 1000U);
			context.writeLine("s");
		}
		lexxos::kernel::sleepMs(intervalMs);
	}

	input->setRawEventReporting(false);

	// Summarize observed input activity.
	context.write("keyboard-live: events=");
	writeUnsigned(context, totalEvents);
	context.write(" changes=");
	writeUnsigned(context, totalChanges);
	context.writeLine("");
	if (totalEvents == 0 && totalChanges == 0) {
		context.writeLine("keyboard-live: no key activity detected");
	}
	return 0;
}

} // namespace lexxos::apps::device_programs
