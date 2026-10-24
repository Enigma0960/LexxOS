#include <lexxos/apps/devices/keyboard_raw_test_program.h>

#include <cstdint>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/devices/detail/input_program_support.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/input_device.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::device_programs {
namespace {

	constexpr lexxos::program::ProgramArgumentSpec keyboardRawArguments[] = {
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
         .name = "name",
         .valueName = "name",
         .help = "input device name",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	};

} // namespace

lexxos::program::ProgramDescriptor KeyboardRawTestProgram::descriptor() {
	return {
	    .name = "keyboard-raw",
	    .help = "run a live raw matrix keyboard test",
	    .description = "Polls an input device and prints raw matrix masks with row/column coordinates.",
	    .usage = "keyboard-raw [-s seconds] [-i milliseconds] [name]",
	    .arguments = argumentList(keyboardRawArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int KeyboardRawTestProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	// Select a ready input device and validate timing options.
	auto const deviceName = arguments.positional("name");
	auto* input = !deviceName.empty() ? context.devices().inputDevice(deviceName) : context.devices().primaryInputDevice();
	if (input == nullptr) {
		context.writeErrorLine("keyboard-raw: input device is not registered");
		return 1;
	}
	if (!input->isReady()) {
		context.writeErrorLine("keyboard-raw: input device is not ready");
		return 1;
	}

	auto inputLock = detail::lockInputDeviceForInteractiveProgram(context, *input, "keyboard-raw");
	if (!inputLock) {
		context.writeErrorLine("keyboard-raw: input device is busy");
		return 1;
	}

	std::uint32_t seconds = 30;
	std::uint32_t intervalMs = 25;
	if (!detail::parseUnsigned(arguments.valueOr("seconds", "30"), seconds) || seconds == 0 || seconds > 3600) {
		context.writeErrorLine("keyboard-raw: invalid seconds value");
		return 1;
	}
	if (!detail::parseUnsigned(arguments.valueOr("interval", "25"), intervalMs) || intervalMs == 0 || intervalMs > 1000) {
		context.writeErrorLine("keyboard-raw: invalid interval value");
		return 1;
	}

	// Print the configuration and initial raw matrix state.
	context.write("keyboard-raw: device=");
	context.write(input->name());
	context.write(" duration=");
	writeUnsigned(context, seconds);
	context.write("s interval=");
	writeUnsigned(context, intervalMs);
	context.writeLine("ms");
	context.writeLine("press one key at a time; use the raw mask and r/c coordinate to update the key map");

	input->update();
	auto previousRaw = input->rawState();
	detail::writeRawKeyboardState(context, *input);

	// Poll raw-state changes until the requested duration expires.
	auto const startedMs = uptimeMs(context);
	auto lastHeartbeatMs = startedMs;
	auto const durationMs = seconds * 1000U;
	std::uint32_t totalChanges = 0;

	while (uptimeMs(context) - startedMs < durationMs) {
		if (context.pollCancellationRequest()) {
			context.writeLine("keyboard-raw: canceled");
			return 130;
		}

		input->update();
		auto const raw = input->rawState();
		if (raw != previousRaw) {
			previousRaw = raw;
			detail::writeRawKeyboardState(context, *input);
			++totalChanges;
		}

		auto const nowMs = uptimeMs(context);
		if (nowMs - lastHeartbeatMs >= 2000U) {
			lastHeartbeatMs = nowMs;
			context.write("raw-live: ");
			writeUnsigned(context, (nowMs - startedMs) / 1000U);
			context.writeLine("s");
		}
		lexxos::kernel::sleepMs(intervalMs);
	}

	// Summarize raw matrix activity.
	context.write("keyboard-raw: changes=");
	writeUnsigned(context, totalChanges);
	context.writeLine("");
	if (totalChanges == 0) {
		context.writeLine("keyboard-raw: no raw matrix activity detected");
	}
	return 0;
}

} // namespace lexxos::apps::device_programs
