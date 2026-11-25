#include <lexxos/apps/gameboy/gb_emu_config_program.h>

namespace lexxos::apps::gameboy_programs {

lexxos::program::ProgramDescriptor GbEmuConfigProgram::descriptor() {
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

int GbEmuConfigProgram::run(
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

bool GbEmuConfigProgram::resolveSettingsPath(
    lexxos::os::AppContext& context,
    lexxos::program::ParsedArguments const& arguments,
    std::array<char, lexxos::vfs::maxPathLength>& path) {
	return lexxos::apps::resolvePath(context, arguments.valueOr("path", detail::gbEmuSettingsPath), path);
}

void GbEmuConfigProgram::writeVfsError(
    lexxos::os::AppContext& context,
    std::string_view path,
    lexxos::vfs::WriteStatus status) {
	context.write("gbemu-config: ");
	context.write(path);
	context.write(": ");
	context.writeLine(lexxos::vfs::toString(status));
}

bool GbEmuConfigProgram::ensureDirectory(lexxos::os::AppContext& context, std::string_view path) {
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

bool GbEmuConfigProgram::ensureDefaultDirectories(lexxos::os::AppContext& context) {
	return ensureDirectory(context, detail::gbEmuSettingsRootDirectory)
	    && ensureDirectory(context, detail::gbEmuSettingsDirectory);
}

int GbEmuConfigProgram::showStatus(lexxos::os::AppContext& context, std::string_view path) {
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

bool GbEmuConfigProgram::writeDefaults(lexxos::os::AppContext& context, std::string_view path, bool force) {
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

} // namespace lexxos::apps::gameboy_programs
