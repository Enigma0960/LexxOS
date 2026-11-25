#pragma once

#include <lexxos/apps/ui/native_ui_tree_demo_program.h>
#include <lexxos/program/program.h>

namespace lexxos::apps::ui_programs {

class NativeUiDemoProgram final {
public:
	static program::ProgramDescriptor descriptor();
	static int run(os::AppContext& context, program::ProgramInvocation const&,
	    program::ParsedArguments const&);
};

} // namespace lexxos::apps::ui_programs
