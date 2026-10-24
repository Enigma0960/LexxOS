#include <lexxos/apps/fs/fs_info_program.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/common/pretty_table.h>
#include <lexxos/feature.h>
#include <lexxos/vfs/vfs.h>

#if FEATURE_ENABLED(FATFS)
	#include <lexxos/fs/fat/fat_volume.h>
#endif

namespace lexxos::apps::filesystem_programs {
namespace {

	constexpr lexxos::apps::PrettyTableColumn vfsColumns[] = {
	    {.name = "table",  .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 5, .maxWidth = 22},
	    { .name = "used", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 4,  .maxWidth = 8},
	    {  .name = "max", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 3,  .maxWidth = 8},
	};

	constexpr lexxos::apps::PrettyTableColumn mountColumns[] = {
	    {.name = "source", .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 6, .maxWidth = 18},
	    {.name = "target", .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 6, .maxWidth = 18},
	    {  .name = "type", .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 4, .maxWidth = 12},
	    { .name = "state", .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 5,  .maxWidth = 9},
	    {  .name = "mode", .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 4,  .maxWidth = 4},
	};

	constexpr lexxos::apps::PrettyTableColumn fatVolumeColumns[] = {
	    {  .name = "source",  .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 6, .maxWidth = 16},
	    {  .name = "target",  .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 6, .maxWidth = 16},
	    {    .name = "type",  .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 4,  .maxWidth = 8},
	    {.name = "clusters", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 8, .maxWidth = 12},
	    {     .name = "spc", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 3,  .maxWidth = 3},
	    {    .name = "mode",  .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 4,  .maxWidth = 4},
	};

	bool vfsTableRow(std::size_t rowIndex, lexxos::apps::PrettyTableRow& row, void* userData) {
		auto const& info = *static_cast<lexxos::vfs::VfsInfo const*>(userData);
		if (rowIndex == 0) {
			row.set(0, "block-device-mounts");
			row.setUnsigned64(1, info.blockDevices);
			row.setUnsigned64(2, info.maxBlockDevices);
			return true;
		}
		if (rowIndex == 1) {
			row.set(0, "filesystem-mounts");
			row.setUnsigned64(1, info.filesystemMounts);
			row.setUnsigned64(2, info.maxFilesystemMounts);
			return true;
		}
		return false;
	}

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

#if FEATURE_ENABLED(FATFS)
	struct FatVolumeLookup {
		std::size_t target{0};
		std::size_t current{0};
		lexxos::apps::PrettyTableRow* row{nullptr};
		bool found{false};
	};

	bool visitFatVolumeForTable(lexxos::fs::fat::FatVolumeInfo const& volume, void* userData) {
		auto& lookup = *static_cast<FatVolumeLookup*>(userData);
		if (lookup.current == lookup.target && lookup.row != nullptr) {
			lookup.row->set(0, volume.source);
			lookup.row->set(1, volume.target);
			lookup.row->set(2, volume.type);
			lookup.row->setUnsigned(3, volume.clusterCount);
			lookup.row->setUnsigned(4, volume.sectorsPerCluster);
			lookup.row->set(5, volume.readOnly ? "ro" : "rw");
			lookup.found = true;
			return false;
		}
		++lookup.current;
		return true;
	}

	bool fatVolumeTableRow(std::size_t rowIndex, lexxos::apps::PrettyTableRow& row, void*) {
		FatVolumeLookup lookup{.target = rowIndex, .row = &row};
		lexxos::fs::fat::visitMountedVolumes(visitFatVolumeForTable, &lookup);
		return lookup.found;
	}

	bool markAnyFatVolume(lexxos::fs::fat::FatVolumeInfo const&, void* userData) {
		*static_cast<bool*>(userData) = true;
		return false;
	}
#endif

} // namespace

lexxos::program::ProgramDescriptor FsInfoProgram::descriptor() {
	return {
	    .name = "fs-info",
	    .help = "show VFS diagnostic information",
	    .description = "Shows VFS mount slots, active mounts, mounted FAT volumes and tmpfs usage.",
	    .usage = "fs-info",
	    .handler = run,
	};
}

int FsInfoProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	auto info = lexxos::vfs::vfsInfo();

	// Print capacity and usage of the VFS mount tables.
	context.writeLine("vfs:");
	lexxos::apps::PrettyTable vfsTable{vfsColumns, sizeof(vfsColumns) / sizeof(vfsColumns[0])};
	vfsTable.print(context, vfsTableRow, &info);

	// Enumerate active VFS mounts.
	context.writeLine("mounts:");
	lexxos::apps::PrettyTable mountTable{mountColumns, sizeof(mountColumns) / sizeof(mountColumns[0])};
	mountTable.print(context, mountTableRow, nullptr);

#if FEATURE_ENABLED(FATFS)
	// Print FAT-specific volume data.
	context.writeLine("fat volumes:");
	bool anyFatVolume = false;
	lexxos::fs::fat::visitMountedVolumes(markAnyFatVolume, &anyFatVolume);
	if (!anyFatVolume) {
		context.writeLine("  none");
	} else {
		lexxos::apps::PrettyTable fatTable{fatVolumeColumns, sizeof(fatVolumeColumns) / sizeof(fatVolumeColumns[0])};
		fatTable.print(context, fatVolumeTableRow, nullptr);
	}
#else
	context.writeLine("fat volumes: disabled");
#endif

	// Finish with the RAM-backed filesystem summary.
	context.writeLine("tmpfs:");
	writeTmpSummary(context, info.tmp);
	return 0;
}

} // namespace lexxos::apps::filesystem_programs
