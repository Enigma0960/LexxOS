#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::text_editor_programs {

class EditProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::text_editor_programs
