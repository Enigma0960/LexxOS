#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::network_programs {

class HostedOtaProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(
	    lexxos::os::AppContext& context,
	    lexxos::program::ProgramInvocation const& invocation,
	    lexxos::program::ParsedArguments const& arguments);
};

class HostedOtaRxProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(
	    lexxos::os::AppContext& context,
	    lexxos::program::ProgramInvocation const& invocation,
	    lexxos::program::ParsedArguments const& arguments);
};

} // namespace lexxos::apps::network_programs
