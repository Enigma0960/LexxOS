#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::storage_programs {

class SdMultiWriteProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::storage_programs
