#include <lexxos/apps/fs/cd_program.h>

#include <array>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::filesystem_programs {
namespace {

	constexpr lexxos::program::ProgramArgumentSpec cdArguments[] = {
	    {
         .name = "path",
         .valueName = "path",
         .help = "optional VFS path",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::Path,
         .required = false,
	     },
	};

} // namespace

lexxos::program::ProgramDescriptor CdProgram::descriptor() {
	return {
	    .name = "cd",
	    .help = "change current VFS directory",
	    .usage = "cd [path]",
	    .arguments = argumentList(cdArguments),
	    .handler = run,
	};
}

int CdProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	// Resolve the requested directory, using the VFS root by default.
	std::array<char, lexxos::vfs::maxPathLength> path{};
	std::string_view const requestedPath = arguments.positionalOr("path", "/");
	if (!resolvePath(context, requestedPath, path)) {
		return 1;
	}

	// Validate that the resolved node exists and is a directory.
	lexxos::vfs::Stat stat;
	if (!lexxos::vfs::stat(path.data(), stat)) {
		context.write("cd: no such file or directory: ");
		context.writeLine(path.data());
		return 1;
	}
	if (stat.type != lexxos::vfs::NodeType::Directory) {
		context.write("cd: not a directory: ");
		context.writeLine(path.data());
		return 1;
	}

	// Commit the new working directory to the application context.
	if (!context.setWorkingDirectory(path.data())) {
		context.writeLine("cd: failed to update working directory");
		return 1;
	}
	return 0;
}

} // namespace lexxos::apps::filesystem_programs
