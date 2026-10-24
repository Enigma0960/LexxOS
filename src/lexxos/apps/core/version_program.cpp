#include <lexxos/apps/core/version_program.h>

#include <lexxos/build_info.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::system_programs {

lexxos::program::ProgramDescriptor VersionProgram::descriptor() {
	return {.name = "version", .help = "show LexxOS build and feature milestone", .description = "Shows LexxOS version, build date, platform, board and the current filesystem feature milestone.", .usage = "version", .handler = run, .legacy = false, .enabled = true};
}

int VersionProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	context.write("version: ");
	context.writeLine(lexxos::version);
	context.write("milestone: ");
	context.writeLine(lexxos::milestone);
	context.write("platform: ");
	context.writeLine(lexxos::platformName);
	context.write("board: ");
	context.writeLine(LEXXOS_BOARD_NAME);
	context.write("flavor: ");
	context.writeLine(lexxos::buildFlavor);
	context.write("target: ");
	context.write(lexxos::buildTargetSystem);
	context.write(" ");
	context.writeLine(lexxos::buildTargetProcessor);
	context.write("host: ");
	context.write(lexxos::buildHostSystem);
	context.write(" ");
	context.writeLine(lexxos::buildHostProcessor);
	context.write("build: ");
	context.write(lexxos::buildDate);
	context.write(" ");
	context.writeLine(lexxos::buildTime);
	context.write("filesystem: ");
	context.writeLine(lexxos::filesystemMilestone);
	context.writeLine("features: vfs, file-handle, stdin/stdout redirects, tmpfs, FatFs FAT32 rw, ASCII LFN");
	return 0;
}

} // namespace lexxos::apps::system_programs
