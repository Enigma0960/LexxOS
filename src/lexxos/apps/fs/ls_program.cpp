#include <lexxos/apps/fs/ls_program.h>

#include <array>
#include <cstdint>
#include <new>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/common/pretty_table.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program_memory_arena.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::filesystem_programs {

using lexxos::program::ProgramDescriptor;
using lexxos::program::ProgramLaunchMode;
using lexxos::program::ProgramMemoryClass;

namespace {

	using PathBuffer = std::array<char, lexxos::vfs::maxPathLength>;

	constexpr std::uint32_t lsWorkspaceBytes = static_cast<std::uint32_t>(lexxos::vfs::maxPathLength);

	constexpr lexxos::apps::PrettyTableColumn lsColumns[] = {
	    {.name = "type",  .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 4,  .maxWidth = 5},
	    {.name = "name",  .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 4, .maxWidth = 32},
	    {.name = "size", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 4, .maxWidth = 12},
	};

	constexpr lexxos::program::ProgramArgumentSpec lsArguments[] = {
	    {
         .name = "path",
         .valueName = "path",
         .help = "optional VFS path",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::Path,
         .required = false,
	     },
	};

	bool allocatePath(lexxos::os::AppContext& context, PathBuffer*& path) {
		auto* const arena = context.memoryArena();
		if (arena == nullptr) {
			context.writeLine("ls: program memory arena is unavailable");
			return false;
		}

		auto* const memory = arena->allocate(sizeof(PathBuffer), alignof(PathBuffer));
		if (memory == nullptr) {
			context.writeLine("ls: not enough program workspace memory");
			return false;
		}

		path = new (memory) PathBuffer{};
		return true;
	}

	struct DirectoryEntryLookup {
		char const* path{nullptr};
		std::size_t target{0};
		std::size_t current{0};
		lexxos::apps::PrettyTableRow* row{nullptr};
		bool found{false};
	};

	bool visitDirectoryEntryForTable(lexxos::vfs::DirectoryEntry const& entry, void* userData) {
		auto& lookup = *static_cast<DirectoryEntryLookup*>(userData);
		if (lookup.current == lookup.target && lookup.row != nullptr) {
			lookup.row->set(0, lexxos::vfs::toString(entry.type));
			lookup.row->set(1, entry.name);
			lookup.row->setUnsigned64(2, entry.sizeBytes);
			lookup.found = true;
			return false;
		}
		++lookup.current;
		return true;
	}

	bool directoryTableRow(std::size_t rowIndex, lexxos::apps::PrettyTableRow& row, void* userData) {
		auto const* path = static_cast<char const*>(userData);
		DirectoryEntryLookup lookup{.path = path, .target = rowIndex, .row = &row};
		lexxos::vfs::list(path, visitDirectoryEntryForTable, &lookup);
		return lookup.found;
	}

	bool markAnyDirectoryEntry(lexxos::vfs::DirectoryEntry const&, void* userData) {
		*static_cast<bool*>(userData) = true;
		return false;
	}

} // namespace

ProgramDescriptor LsProgram::descriptor() {
	return {
	    .name = "ls",
	    .help = "list VFS directory",
	    .usage = "ls [path]",
	    .arguments = argumentList(lsArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 8192,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = lsWorkspaceBytes,
	                .cancellable = true,
	                .allowBackground = false,
	                },
	};
}

int LsProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	PathBuffer* path = nullptr;
	if (!allocatePath(context, path)) {
		return 1;
	}

	// Resolve the explicit path or current working directory.
	if (!resolvePath(context, arguments.positionalOr("path", context.workingDirectory()), *path)) {
		return 1;
	}

	// Print a non-directory target directly.
	lexxos::vfs::Stat stat;
	if (!lexxos::vfs::stat(path->data(), stat)) {
		context.write("ls: cannot access: ");
		context.writeLine(path->data());
		return 1;
	}
	if (stat.type != lexxos::vfs::NodeType::Directory) {
		context.write(lexxos::vfs::toString(stat.type));
		context.write(" ");
		context.writeLine(path->data());
		return 0;
	}

	// Enumerate directory entries with a compact type prefix.
	bool anyEntry = false;
	lexxos::vfs::list(path->data(), markAnyDirectoryEntry, &anyEntry);
	if (anyEntry) {
		lexxos::apps::PrettyTable table{lsColumns, sizeof(lsColumns) / sizeof(lsColumns[0])};
		table.print(context, directoryTableRow, path->data());
	}
	return 0;
}

} // namespace lexxos::apps::filesystem_programs
