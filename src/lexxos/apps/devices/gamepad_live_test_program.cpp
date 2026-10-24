#include "gamepad_live_test_program.h"
#include <lexxos/apps/devices/gamepad_test_program.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/devices/detail/input_program_support.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/gamepad_device.h>
#include <lexxos/kernel/system.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/arguments.h>

namespace lexxos::apps::device_programs {

namespace {

	constexpr lexxos::program::ProgramArgumentSpec gamepadLiveArguments[] = {
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
         .help = "gamepad device name",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	};

	void writeGamepadState(lexxos::os::AppContext& context, lexxos::devices::GamepadState const& state) {
		detail::writeBoolField(context, "up", state.up);
		detail::writeBoolField(context, "down", state.down);
		detail::writeBoolField(context, "left", state.left);
		detail::writeBoolField(context, "right", state.right);
		detail::writeBoolField(context, "a", state.a);
		detail::writeBoolField(context, "b", state.b);
		detail::writeBoolField(context, "x", state.x);
		detail::writeBoolField(context, "y", state.y);
		detail::writeBoolField(context, "start", state.start);
		detail::writeBoolField(context, "select", state.select);
		context.writeLine("");
	}

	[[nodiscard]] std::uint16_t gamepadStateMask(lexxos::devices::GamepadState const& state) {
		return static_cast<std::uint16_t>((state.up ? 1U << 0U : 0U) | (state.down ? 1U << 1U : 0U) | (state.left ? 1U << 2U : 0U) | (state.right ? 1U << 3U : 0U) | (state.a ? 1U << 4U : 0U) | (state.b ? 1U << 5U : 0U) | (state.start ? 1U << 6U : 0U) | (state.select ? 1U << 7U : 0U) | (state.x ? 1U << 8U : 0U) | (state.y ? 1U << 9U : 0U));
	}

} // namespace

lexxos::program::ProgramDescriptor GamepadLiveTestProgram::descriptor() {
	return {
	    .name = "gamepad-live",
	    .help = "run a live gamepad input test",
	    .description = "Polls a gamepad for a limited time and prints every state change.",
	    .usage = "gamepad-live [-s seconds] [-i milliseconds] [name]",
	    .arguments = argumentList(gamepadLiveArguments),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int GamepadLiveTestProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto const deviceName = arguments.positional("name");
	auto* gamepad = !deviceName.empty() ? context.devices().gamepadDevice(deviceName) : context.devices().primaryGamepadDevice();
	if (gamepad == nullptr) {
		context.writeErrorLine("gamepad-live: gamepad device is not registered");
		return 1;
	}
	if (!gamepad->isReady()) {
		context.writeErrorLine("gamepad-live: gamepad device is not ready");
		return 1;
	}

	auto gamepadLock = context.devices().lockGamepadDevice(gamepad->name(), lexxos::devices::DeviceResourcePriority::Interactive, "gamepad-live");
	if (!gamepadLock) {
		context.writeErrorLine("gamepad-live: gamepad device is busy");
		return 1;
	}

	std::uint32_t seconds = 30;
	std::uint32_t intervalMs = 25;
	if (!detail::parseUnsigned(arguments.valueOr("seconds", "30"), seconds) || seconds == 0 || seconds > 3600) {
		context.writeErrorLine("gamepad-live: invalid seconds value");
		return 1;
	}
	if (!detail::parseUnsigned(arguments.valueOr("interval", "25"), intervalMs) || intervalMs == 0 || intervalMs > 1000) {
		context.writeErrorLine("gamepad-live: invalid interval value");
		return 1;
	}

	context.write("gamepad-live: device=");
	context.write(gamepad->name());
	context.write(" duration=");
	writeUnsigned(context, seconds);
	context.write("s interval=");
	writeUnsigned(context, intervalMs);
	context.writeLine("ms");
	context.writeLine("press and release every direction and button");

	auto state = gamepad->state();
	auto previousMask = gamepadStateMask(state);
	writeGamepadState(context, state);

	auto const startedMs = uptimeMs(context);
	auto const durationMs = seconds * 1000U;
	std::uint32_t changes = 0;
	while (uptimeMs(context) - startedMs < durationMs) {
		if (context.pollCancellationRequest()) {
			context.writeLine("gamepad-live: canceled");
			return 130;
		}

		state = gamepad->state();
		auto const mask = gamepadStateMask(state);
		if (mask != previousMask) {
			previousMask = mask;
			writeGamepadState(context, state);
			++changes;
		}
		sleepMs(context, intervalMs);
	}

	context.write("gamepad-live: changes=");
	writeUnsigned(context, changes);
	context.writeLine("");
	if (changes == 0) {
		context.writeLine("gamepad-live: no gamepad activity detected");
	}
	return 0;
}

} // namespace lexxos::apps::device_programs
