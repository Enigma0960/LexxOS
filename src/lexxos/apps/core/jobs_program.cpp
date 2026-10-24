#include <lexxos/apps/core/jobs_program.h>

#include <cstring>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/common/pretty_table.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program_supervisor.h>

namespace lexxos::apps::system_programs {

namespace {

	bool equals(char const* value, char const* expected) {
		return value != nullptr && std::strcmp(value, expected) == 0;
	}

	constexpr lexxos::apps::PrettyTableColumn jobColumns[] = {
	    {      .name = "id", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 2,  .maxWidth = 4},
	    {   .name = "state",  .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 5, .maxWidth = 14},
	    { .name = "program",  .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 7, .maxWidth = 14},
	    {   .name = "iface",  .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 5,  .maxWidth = 9},
	    {   .name = "stack", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 5, .maxWidth = 10},
	    {    .name = "free", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 4, .maxWidth = 10},
	    {.name = "priority", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 8,  .maxWidth = 8},
	    {  .name = "memory",  .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 6, .maxWidth = 13},
	    {     .name = "int", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 3, .maxWidth = 11},
	    {     .name = "ext", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 3, .maxWidth = 11},
	    {    .name = "exit", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 4,  .maxWidth = 5},
	    {  .name = "launch",  .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 6, .maxWidth = 14},
	};

	struct JobLookup {
		std::size_t target{0};
		std::size_t current{0};
		lexxos::apps::PrettyTableRow* row{nullptr};
		bool found{false};
	};

	void countProcess(lexxos::program::ProgramProcessSnapshot const&, void*) {
	}

	void visitProcessForTable(lexxos::program::ProgramProcessSnapshot const& process, void* userData) {
		auto& lookup = *static_cast<JobLookup*>(userData);
		if (lookup.current == lookup.target && lookup.row != nullptr) {
			lookup.row->setUnsigned(0, process.id);
			lookup.row->set(1, lexxos::program::toString(process.state));
			lookup.row->set(2, process.name);
			lookup.row->set(3, lexxos::program::toString(process.interfaceKind));
			lookup.row->setUnsigned(4, process.stackSizeBytes);
			lookup.row->setUnsigned(5, process.stackFreeBytes);
			lookup.row->setUnsigned(6, process.priority);
			lookup.row->set(7, lexxos::program::toString(process.memoryClass));
			lookup.row->appendUnsigned(8, process.internalMemoryReservedBytes);
			lookup.row->append(8, "/");
			lookup.row->appendUnsigned(8, process.internalMemoryQuotaBytes);
			lookup.row->appendUnsigned(9, process.externalMemoryReservedBytes);
			lookup.row->append(9, "/");
			lookup.row->appendUnsigned(9, process.externalMemoryQuotaBytes);
			lookup.row->setUnsigned(10, static_cast<std::uint32_t>(process.exitCode < 0 ? 0 : process.exitCode));
			lookup.row->set(11, lexxos::program::toString(process.launchStatus));
			lookup.found = true;
		}
		++lookup.current;
	}

	bool jobTableRow(std::size_t rowIndex, lexxos::apps::PrettyTableRow& row, void*) {
		JobLookup lookup{.target = rowIndex, .row = &row};
		bool any = false;
		lexxos::program::supervisor().visitProcesses(visitProcessForTable, &lookup, any);
		return lookup.found;
	}

} // namespace

lexxos::program::ProgramDescriptor JobsProgram::descriptor() {
	return {
	    .name = "jobs",
	    .help = "show supervised program processes",
	    .description = "Prints the ProgramSupervisor process table used by run/stop.",
	    .usage = "jobs [clear]",
	    .handler = run,
	    .allowExtraArguments = true,
	    .legacy = false,
	    .enabled = true,
	};
}

int JobsProgram::run(lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const& invocation,
    lexxos::program::ParsedArguments const&) {
	if (invocation.argc > 1) {
		if (invocation.argc == 2 && equals(invocation.argv[1], "clear")) {
			auto const cleared = lexxos::program::supervisor().clearFinished();
			context.write("jobs: cleared ");
			writeUnsigned(context, static_cast<std::uint32_t>(cleared));
			context.writeLine(" finished processes");
			return 0;
		}
		context.writeErrorLine("usage: jobs [clear]");
		return 2;
	}

	bool any = false;
	lexxos::program::supervisor().visitProcesses(countProcess, nullptr, any);
	if (!any) {
		context.writeLine("no supervised processes");
		return 0;
	}

	lexxos::apps::PrettyTable table{jobColumns, sizeof(jobColumns) / sizeof(jobColumns[0])};
	table.setMaxTableWidth(0);
	table.print(context, jobTableRow, nullptr);
	return 0;
}

} // namespace lexxos::apps::system_programs
