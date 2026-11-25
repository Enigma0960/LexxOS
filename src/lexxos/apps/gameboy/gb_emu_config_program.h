#pragma once

#include <array>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/gameboy/detail/gameboy_settings.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::gameboy_programs {

class GbEmuConfigProgram final {
public:
	static lexxos::program::ProgramDescriptor descriptor();

	static int run(
	    lexxos::os::AppContext& context,
	    lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const& arguments);

private:
	static constexpr lexxos::program::ProgramArgumentSpec arguments[] = {
	    {
	     .name = "path",
	     .valueName = "path",
	     .help = "settings file path; default is /mnt/sd/apps/gbemu/settings.ini",
	     .kind = lexxos::program::ProgramArgumentKind::Option,
	     .valueType = lexxos::program::ProgramArgumentValueType::Path,
	     .required = false,
	     .defaultValue = detail::gbEmuSettingsPath,
	     },
	    {
	     .name = "init",
	     .help = "create the settings file if it is missing",
	     .kind = lexxos::program::ProgramArgumentKind::Flag,
	     .required = false,
	     },
	    {
	     .name = "force",
	     .help = "overwrite an existing settings file with defaults",
	     .kind = lexxos::program::ProgramArgumentKind::Flag,
	     .required = false,
	     },
	    {
	     .name = "show-defaults",
	     .help = "print default settings without writing a file",
	     .kind = lexxos::program::ProgramArgumentKind::Flag,
	     .required = false,
	     },
	};

	static bool resolveSettingsPath(
	    lexxos::os::AppContext& context,
	    lexxos::program::ParsedArguments const& arguments,
	    std::array<char, lexxos::vfs::maxPathLength>& path);

	static void writeVfsError(
	    lexxos::os::AppContext& context,
	    std::string_view path,
	    lexxos::vfs::WriteStatus status);

	static bool ensureDirectory(lexxos::os::AppContext& context, std::string_view path);

	static bool ensureDefaultDirectories(lexxos::os::AppContext& context);

	static int showStatus(lexxos::os::AppContext& context, std::string_view path);

	static bool writeDefaults(lexxos::os::AppContext& context, std::string_view path, bool force);
};

} // namespace lexxos::apps::gameboy_programs
