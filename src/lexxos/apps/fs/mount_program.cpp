#include <lexxos/apps/fs/mount_program.h>

#include <array>
#include <cstdint>
#include <new>
#include <string_view>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/common/pretty_table.h>
#include <lexxos/fs/fat/fat_volume.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program_memory_arena.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::filesystem_programs {

using lexxos::program::ProgramDescriptor;
using lexxos::program::ProgramLaunchMode;
using lexxos::program::ProgramMemoryClass;

namespace {

	using PathBuffer = std::array<char, lexxos::vfs::maxPathLength>;

	constexpr std::uint32_t mountWorkspaceBytes = static_cast<std::uint32_t>(lexxos::vfs::maxPathLength * 2U);

	constexpr lexxos::apps::PrettyTableColumn mountColumns[] = {
	    {.name = "source", .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 6, .maxWidth = 18},
	    {.name = "target", .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 6, .maxWidth = 18},
	    {  .name = "type", .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 4, .maxWidth = 12},
	    { .name = "state", .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 5,  .maxWidth = 9},
	    {  .name = "mode", .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 4,  .maxWidth = 4},
	};

	constexpr lexxos::program::ProgramArgumentSpec mountArguments[] = {
	    {
         .name = "source",
         .valueName = "source",
         .help = "block device or VFS source path",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::Path,
         .required = false,
	     },
	    {
         .name = "target",
         .valueName = "target",
         .help = "VFS mount target path",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::Path,
         .required = false,
	     },
	    {
         .name = "type",
         .valueName = "type",
         .help = "filesystem type, default: fat",
         .kind = lexxos::program::ProgramArgumentKind::Positional,
         .valueType = lexxos::program::ProgramArgumentValueType::String,
         .required = false,
	     },
	};

	struct MountLookup {
		std::size_t target{0};
		std::size_t current{0};
		lexxos::apps::PrettyTableRow* row{nullptr};
		bool found{false};
	};

	bool visitMountForTable(lexxos::vfs::MountInfo const& mount, void* userData) {
		auto& lookup = *static_cast<MountLookup*>(userData);
		if (lookup.current == lookup.target && lookup.row != nullptr) {
			lookup.row->set(0, mount.source);
			lookup.row->set(1, mount.target);
			lookup.row->set(2, mount.type);
			lookup.row->set(3, mount.ready ? "ready" : "not-ready");
			lookup.row->set(4, mount.readOnly ? "ro" : "rw");
			lookup.found = true;
			return false;
		}
		++lookup.current;
		return true;
	}

	bool mountTableRow(std::size_t rowIndex, lexxos::apps::PrettyTableRow& row, void*) {
		MountLookup lookup{.target = rowIndex, .row = &row};
		lexxos::vfs::visitMounts(visitMountForTable, &lookup);
		return lookup.found;
	}

	template <typename T>
	bool allocateWorkspaceObject(lexxos::os::AppContext& context, T*& output) {
		auto* const arena = context.memoryArena();
		if (arena == nullptr) {
			context.writeLine("mount: program memory arena is unavailable");
			return false;
		}

		auto* const memory = arena->allocate(sizeof(T), alignof(T));
		if (memory == nullptr) {
			context.writeLine("mount: not enough program workspace memory");
			return false;
		}

		output = new (memory) T{};
		return true;
	}

	bool allocateWorkspace(lexxos::os::AppContext& context, PathBuffer*& source, PathBuffer*& target) {
		return allocateWorkspaceObject(context, source) && allocateWorkspaceObject(context, target);
	}

} // namespace

ProgramDescriptor MountProgram::descriptor() {
	return {
	    .name = "mount",
	    .help = "show or create VFS mounts",
	    .usage = "mount [<source> <target> [fat]]",
	    .arguments = argumentList(mountArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 4096,
	                .priority = 1,
	                .memoryClass = ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = mountWorkspaceBytes,
	                .cancellable = true,
	                .allowBackground = false,
	                },
	};
}

int MountProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	// With no arguments, list every registered mount.
	if (arguments.positionalCount() == 0) {
		lexxos::apps::PrettyTable table{mountColumns, sizeof(mountColumns) / sizeof(mountColumns[0])};
		table.print(context, mountTableRow, nullptr);
		return 0;
	}

	// Validate the mount request and supported filesystem aliases.
	if (arguments.positionalCount() != 2 && arguments.positionalCount() != 3) {
		context.writeLine("usage: mount <source> <target> [fat]");
		return 1;
	}

	std::string_view const type = arguments.positionalOr("type", "fat");
	if (type != "fat" && type != "fat16" && type != "fat32") {
		context.write("mount: unsupported filesystem type: ");
		context.writeLine(type);
		return 1;
	}

	PathBuffer* source = nullptr;
	PathBuffer* target = nullptr;
	if (!allocateWorkspace(context, source, target)) {
		return 1;
	}

	// Resolve source and target paths while preserving block-device names.
	std::string_view sourceView = arguments.positional("source");
	if (!context.vfs().blockDevice(sourceView)) {
		if (!resolvePath(context, sourceView, *source)) {
			return 1;
		}
		sourceView = source->data();
	}
	if (!resolvePath(context, arguments.positional("target"), *target)) {
		return 1;
	}

	// Serialize SD access and prepare the card before mounting its block device.
	auto sdMountLock = lexxos::devices::DeviceResourceLock{};
	if (isSdBlockSource(sourceView)) {
		sdMountLock = lockSdBlockDevice(context, "mount");
		if (!sdMountLock) {
			return 1;
		}
		if (!prepareSdCardForMount(context)) {
			context.write("mount: ");
			context.write(sourceView);
			context.write(" on ");
			context.write(target->data());
			context.writeLine(": block device is not ready");
			return 1;
		}
	}

	// Mount the FAT volume and print the resulting association.
	auto const status = lexxos::fs::fat::mount(context.vfs(), sourceView, target->data());
	if (status != lexxos::fs::fat::FatMountStatus::Ok) {
		context.write("mount: ");
		context.write(sourceView);
		context.write(" on ");
		context.write(target->data());
		context.write(": ");
		context.writeLine(lexxos::fs::fat::toString(status));
		return 1;
	}

	context.write("mounted ");
	context.write(sourceView);
	context.write(" on ");
	context.write(target->data());
	context.writeLine(" type fat");
	return 0;
}

} // namespace lexxos::apps::filesystem_programs
