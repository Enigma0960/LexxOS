#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::rtc_programs {

class RtcConfigProgram {
public:
	[[nodiscard]] static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext& context,
	    lexxos::program::ProgramInvocation const& invocation,
	    lexxos::program::ParsedArguments const& arguments);
};

} // namespace lexxos::apps::rtc_programs
