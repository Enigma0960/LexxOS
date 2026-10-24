#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::i2c_programs {

class PicoCalcInfoProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::i2c_programs
