#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::music::midi {

class MidiPlayProgram {
public:
	static lexxos::program::ProgramDescriptor descriptor();

private:
	static int run(
	    lexxos::os::AppContext& context,
	    lexxos::program::ProgramInvocation const& invocation,
	    lexxos::program::ParsedArguments const& arguments);
};

} // namespace lexxos::apps::music::midi
