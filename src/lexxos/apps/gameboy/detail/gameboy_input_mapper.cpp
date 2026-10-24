#include <lexxos/apps/gameboy/detail/gameboy_program_arguments.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_support.h>

namespace lexxos::apps::gameboy_programs {

lexxos::gameboy::JoypadState toJoypadState(lexxos::devices::GamepadState const& state) {
	return {
	    .right = state.right,
	    .left = state.left,
	    .up = state.up,
	    .down = state.down,
	    .a = state.a,
	    .b = state.b,
	    .select = state.select,
	    .start = state.start,
	};
}

void writeGamepadState(lexxos::os::AppContext& context, lexxos::devices::GamepadState const& state) {
	context.write("gamepad: ");
	context.write("up:");
	context.write(state.up ? "1" : "0");
	context.write(" down:");
	context.write(state.down ? "1" : "0");
	context.write(" left:");
	context.write(state.left ? "1" : "0");
	context.write(" right:");
	context.write(state.right ? "1" : "0");
	context.write(" a:");
	context.write(state.a ? "1" : "0");
	context.write(" b:");
	context.write(state.b ? "1" : "0");
	context.write(" start:");
	context.write(state.start ? "1" : "0");
	context.write(" select:");
	context.write(state.select ? "1" : "0");
	context.writeLine("");
}

} // namespace lexxos::apps::gameboy_programs
