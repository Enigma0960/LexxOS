#include <lexxos/apps/devices/gamepad_test_program.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/devices/detail/input_program_support.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/gamepad_device.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::device_programs {
namespace {

	constexpr lexxos::program::ProgramArgumentSpec optionalNameArgument[] = {
	    {
         .name = "name",
         .valueName = "name",
         .help = "device name",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	};

} // namespace

lexxos::program::ProgramDescriptor GamepadTestProgram::descriptor() {
	return {
	    .name = "gamepad-test",
	    .help = "print current gamepad state",
	    .description = "Reads one gamepad state sample. Without a name it uses the primary gamepad.",
	    .usage = "gamepad-test [name]",
	    .arguments = argumentList(optionalNameArgument),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int GamepadTestProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	auto const deviceName = arguments.positional("name");
	auto* gamepad = !deviceName.empty() ? context.devices().gamepadDevice(deviceName) : context.devices().primaryGamepadDevice();
	if (gamepad == nullptr) {
		context.writeErrorLine("gamepad-test: gamepad device is not registered");
		return 1;
	}

	auto gamepadLock = context.devices().lockGamepadDevice(gamepad->name(), lexxos::devices::DeviceResourcePriority::Interactive, "gamepad-test");
	if (!gamepadLock) {
		context.writeErrorLine("gamepad-test: gamepad device is busy");
		return 1;
	}

	auto const state = gamepad->state();
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
	return 0;
}

} // namespace lexxos::apps::device_programs
