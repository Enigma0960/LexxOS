#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::ui_programs {

class NativeUiTreeDemoProgram final {
public:
	static program::ProgramDescriptor descriptor();
	static int run(os::AppContext& context, program::ProgramInvocation const&,
	    program::ParsedArguments const&);
};

} // namespace lexxos::apps::ui_programs
