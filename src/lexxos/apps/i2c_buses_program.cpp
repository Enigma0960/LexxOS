#include <lexxos/apps/i2c_buses_program.h>

#include <lexxos/apps/detail/i2c_program_support.h>

namespace lexxos::apps::i2c_programs {

using lexxos::program::ProgramDescriptor;

ProgramDescriptor I2cBusesProgram::descriptor() {
	return {
	    .name = "i2c-buses",
	    .help = "list registered I2C buses",
	    .description = "Lists I2C buses registered in the LexxOS device manager.",
	    .usage = "i2c-buses",
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int I2cBusesProgram::run(lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	detail::writeI2cBusList(context);
	return 0;
}

} // namespace lexxos::apps::i2c_programs
