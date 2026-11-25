#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::system_programs {

/**
 * @lexxos-command-doc autostart
 * @source src/lexxos/apps/core/autostart_program.cpp
 * @description
 * Inspects autostart configuration and sets the global startup delay.
 * @parameters
 * - status: show configuration and storage path
 * - delay <milliseconds>: persist the global delay
 * - enable / disable: change whether shortcuts run at boot
 * @examples
 * ```text
 * autostart status
 * autostart delay 2000
 * ```
 * @end
 */
class AutostartProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::system_programs
