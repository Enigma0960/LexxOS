#include <lexxos/apps/devices/input_test_program.h>

#include <cstdint>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/devices/detail/input_program_support.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/input_device.h>
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

lexxos::program::ProgramDescriptor InputTestProgram::descriptor() {
	return {
	    .name = "input-test",
	    .help = "print pending input events",
	    .description = "Reads pending input events from the primary input device or from the selected input device.",
	    .usage = "input-test [name]",
	    .arguments = argumentList(optionalNameArgument),
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int InputTestProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	// Select and update the requested input device.
	auto const deviceName = arguments.positional("name");
	auto* input = !deviceName.empty() ? context.devices().inputDevice(deviceName) : context.devices().primaryInputDevice();
	if (input == nullptr) {
		context.writeErrorLine("input-test: input device is not registered");
		return 1;
	}

	auto inputLock = detail::lockInputDeviceForInteractiveProgram(context, *input, "input-test");
	if (!inputLock) {
		context.writeErrorLine("input-test: input device is busy");
		return 1;
	}

	input->update();

	// Print the currently queued events.
	lexxos::devices::InputEvent event;
	bool hasEvents = false;
	for (std::uint32_t index = 0; index < 8; ++index) {
		if (!input->pollEvent(event)) {
			break;
		}

		hasEvents = true;
		context.write(lexxos::devices::toString(event.type));
		context.write(": ");
		context.write(lexxos::devices::toString(event.key));
		detail::writeInputEventDetails(context, event);
		context.write(" at ");
		writeUnsigned(context, event.timestampMs);
		context.writeLine(" ms");
	}

	if (!hasEvents) {
		context.writeLine("no pending input events");
	}
	return 0;
}

} // namespace lexxos::apps::device_programs
