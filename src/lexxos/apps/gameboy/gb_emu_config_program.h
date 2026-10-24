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
	static lexxos::program::ProgramDescriptor descriptor() {
		return {
		    .name = "gbemu-config",
		    .help = "show or initialize Game Boy emulator settings",
		    .description = "Creates the default gbemu settings file used by the screen-oriented ROM launcher and prints the same defaults for manual editing.",
		    .usage = "gbemu-config [--path <path>] [--init] [--force] [--show-defaults]",
		    .arguments = lexxos::apps::argumentList(arguments),
		    .handler = run,
		    .runtime = {
		                .launchMode = lexxos::program::ProgramLaunchMode::Inline,
		                .stackSizeBytes = 2048,
		                .priority = 1,
		                .memoryClass = lexxos::program::ProgramMemoryClass::InternalOnly,
		                .externalMemoryQuotaBytes = 0,
		                .internalMemoryQuotaBytes = 0,
		                .cancellable = true,
		                .allowBackground = false,
		                },
		    .legacy = false,
		    .enabled = true,
		};
	}

	static int run(
	    lexxos::os::AppContext& context,
	    lexxos::program::ProgramInvocation const&,
	    lexxos::program::ParsedArguments const& arguments) {
		if (arguments.has("show-defaults")) {
			context.write(detail::gbEmuDefaultSettingsContent);
			return 0;
		}

		std::array<char, lexxos::vfs::maxPathLength> path{};
		if (!resolveSettingsPath(context, arguments, path)) {
			return 1;
		}

		auto const shouldWrite = arguments.has("init") || arguments.has("force");
		if (!shouldWrite) {
			return showStatus(context, path.data());
		}

		if (!arguments.has("path") && !ensureDefaultDirectories(context)) {
			return 1;
		}

		return writeDefaults(context, path.data(), arguments.has("force")) ? 0 : 1;
	}

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
	    std::array<char, lexxos::vfs::maxPathLength>& path) {
		return lexxos::apps::resolvePath(context, arguments.valueOr("path", detail::gbEmuSettingsPath), path);
	}

	static void writeVfsError(
	    lexxos::os::AppContext& context,
	    std::string_view path,
	    lexxos::vfs::WriteStatus status) {
		context.write("gbemu-config: ");
		context.write(path);
		context.write(": ");
		context.writeLine(lexxos::vfs::toString(status));
	}

	static bool ensureDirectory(lexxos::os::AppContext& context, std::string_view path) {
		lexxos::vfs::Stat stat{};
		if (lexxos::vfs::stat(path, stat)) {
			if (stat.type == lexxos::vfs::NodeType::Directory) {
				return true;
			}
			context.write("gbemu-config: ");
			context.write(path);
			context.writeLine(": exists and is not a directory");
			return false;
		}

		auto const status = lexxos::vfs::createDirectory(path);
		if (status == lexxos::vfs::WriteStatus::Ok) {
			return true;
		}

		writeVfsError(context, path, status);
		return false;
	}

	static bool ensureDefaultDirectories(lexxos::os::AppContext& context) {
		return ensureDirectory(context, detail::gbEmuSettingsRootDirectory)
		    && ensureDirectory(context, detail::gbEmuSettingsDirectory);
	}

	static int showStatus(lexxos::os::AppContext& context, std::string_view path) {
		lexxos::vfs::Stat stat{};
		auto const exists = lexxos::vfs::stat(path, stat) && stat.type != lexxos::vfs::NodeType::Missing;
		context.write("gbemu-config: path=");
		context.writeLine(path);
		context.write("gbemu-config: exists=");
		context.writeLine(exists ? "yes" : "no");
		if (exists) {
			context.write("gbemu-config: type=");
			context.writeLine(lexxos::vfs::toString(stat.type));
			context.write("gbemu-config: size=");
			lexxos::apps::writeUnsigned64(context, stat.sizeBytes);
			context.writeLine(" bytes");
		} else {
			context.writeLine("gbemu-config: use --init to create the default file");
		}
		context.writeLine("gbemu-config: use --show-defaults to print the default file body");
		return 0;
	}

	static bool writeDefaults(lexxos::os::AppContext& context, std::string_view path, bool force) {
		lexxos::vfs::Stat stat{};
		if (lexxos::vfs::stat(path, stat) && stat.type != lexxos::vfs::NodeType::Missing) {
			if (stat.type == lexxos::vfs::NodeType::Directory) {
				context.write("gbemu-config: ");
				context.write(path);
				context.writeLine(": path is a directory");
				return false;
			}
			if (!force) {
				context.write("gbemu-config: ");
				context.write(path);
				context.writeLine(": already exists; use --force to overwrite");
				return true;
			}
		}

		auto const writeStatus = lexxos::vfs::writeText(path, detail::gbEmuDefaultSettingsContent, lexxos::vfs::WriteMode::CreateTruncate);
		if (writeStatus != lexxos::vfs::WriteStatus::Ok) {
			writeVfsError(context, path, writeStatus);
			return false;
		}

		auto const syncStatus = lexxos::vfs::sync();
		if (syncStatus != lexxos::vfs::SyncStatus::Ok) {
			context.write("gbemu-config: sync: ");
			context.writeLine(lexxos::vfs::toString(syncStatus));
			return false;
		}

		context.write("gbemu-config: written ");
		context.writeLine(path);
		return true;
	}
};

} // namespace lexxos::apps::gameboy_programs
