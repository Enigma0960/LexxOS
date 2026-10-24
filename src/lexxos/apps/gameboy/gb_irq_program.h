#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::gameboy_programs {

class GbIrqProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&, lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::gameboy_programs
