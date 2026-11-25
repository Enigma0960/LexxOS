#pragma once

#include <lexxos/program/program.h>

namespace lexxos::apps::filesystem_programs {

/**
 * @lexxos-command-doc link
 * @source src/lexxos/apps/fs/link_program.cpp
 * @description
 * Creates and runs portable .lnk program shortcuts (not FAT symbolic links).
 * @parameters
 * - create <path.lnk> <target> [--bg|--fg] [--delay-ms N] [-- args...]
 * - show <path.lnk>
 * - run <path.lnk> [extra arguments...]
 * @examples
 * ```text
 * link create /sys/autostart/game.lnk /mnt/sd/apps/game.lexx --fg --delay-ms 500
 * link run /sys/autostart/game.lnk
 * ```
 * @end
 */
class LinkProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();
	static int run(lexxos::os::AppContext&, lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const&);
};

} // namespace lexxos::apps::filesystem_programs
