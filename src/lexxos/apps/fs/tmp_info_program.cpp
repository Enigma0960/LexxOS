#include <lexxos/apps/fs/tmp_info_program.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/common/pretty_table.h>
#include <lexxos/vfs/vfs.h>

namespace lexxos::apps::filesystem_programs {
namespace {

	constexpr lexxos::apps::PrettyTableColumn tmpFileColumns[] = {
	    {.name = "name",  .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 4, .maxWidth = 32},
	    {.name = "size", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 4, .maxWidth = 12},
	    {.name = "unit",  .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 4,  .maxWidth = 5},
	};

	struct TmpFileLookup {
		std::size_t target{0};
		std::size_t current{0};
		lexxos::apps::PrettyTableRow* row{nullptr};
		bool found{false};
	};

	bool visitTmpFileForTable(lexxos::vfs::DirectoryEntry const& entry, void* userData) {
		auto& lookup = *static_cast<TmpFileLookup*>(userData);
		if (lookup.current == lookup.target && lookup.row != nullptr) {
			lookup.row->set(0, entry.name);
			lookup.row->setUnsigned64(1, entry.sizeBytes);
			lookup.row->set(2, "bytes");
			lookup.found = true;
			return false;
		}
		++lookup.current;
		return true;
	}

	bool tmpFileTableRow(std::size_t rowIndex, lexxos::apps::PrettyTableRow& row, void*) {
		TmpFileLookup lookup{.target = rowIndex, .row = &row};
		lexxos::vfs::list("/tmp", visitTmpFileForTable, &lookup);
		return lookup.found;
	}

	bool markAnyTmpFile(lexxos::vfs::DirectoryEntry const&, void* userData) {
		*static_cast<bool*>(userData) = true;
		return false;
	}

} // namespace

lexxos::program::ProgramDescriptor TmpInfoProgram::descriptor() {
	return {
	    .name = "tmp-info",
	    .help = "show /tmp diagnostic information",
	    .description = "Shows RAM-backed /tmp usage and currently allocated files.",
	    .usage = "tmp-info",
	    .handler = run,
	};
}

int TmpInfoProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	// Print aggregate tmpfs capacity and usage.
	writeTmpSummary(context, lexxos::vfs::tmpInfo());

	// Enumerate allocated files.
	context.writeLine("files:");
	bool anyFile = false;
	lexxos::vfs::list("/tmp", markAnyTmpFile, &anyFile);
	if (!anyFile) {
		context.writeLine("  none");
	} else {
		lexxos::apps::PrettyTable table{tmpFileColumns, sizeof(tmpFileColumns) / sizeof(tmpFileColumns[0])};
		table.print(context, tmpFileTableRow, nullptr);
	}
	return 0;
}

} // namespace lexxos::apps::filesystem_programs
