#include <lexxos/apps/core/tasks_program.h>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/common/pretty_table.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/app_context.h>

namespace lexxos::apps::system_programs {
namespace {

	constexpr lexxos::apps::PrettyTableColumn taskColumns[] = {
	    {    .name = "name",  .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 4, .maxWidth = 18},
	    {.name = "priority", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 8,  .maxWidth = 8},
	    {   .name = "stack", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 5, .maxWidth = 10},
	    {    .name = "free", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 4, .maxWidth = 10},
	    { .name = "started",  .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 7,  .maxWidth = 7},
	    {   .name = "class",  .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 5, .maxWidth = 10},
	    {    .name = "core", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 4,  .maxWidth = 4},
	};

	struct TaskLookup {
		std::size_t target{0};
		std::size_t current{0};
		lexxos::apps::PrettyTableRow* row{nullptr};
		bool found{false};
	};

	void visitTaskForTable(lexxos::kernel::TaskSnapshot const& task, void* userData) {
		auto& lookup = *static_cast<TaskLookup*>(userData);
		if (lookup.current == lookup.target && lookup.row != nullptr) {
			lookup.row->set(0, task.name);
			lookup.row->setUnsigned(1, task.priority);
			lookup.row->setUnsigned(2, task.stackSizeBytes);
			lookup.row->setUnsigned(3, task.stackFreeBytes);
			lookup.row->set(4, task.started ? "yes" : "no");
			lookup.row->set(5, lexxos::kernel::toString(task.executionClass));
			if (task.coreAffinity < 0) {
				lookup.row->set(6, "any");
			} else {
				lookup.row->setUnsigned(6, static_cast<std::uint32_t>(task.coreAffinity));
			}
			lookup.found = true;
		}
		++lookup.current;
	}

	bool taskTableRow(std::size_t rowIndex, lexxos::apps::PrettyTableRow& row, void*) {
		TaskLookup lookup{.target = rowIndex, .row = &row};
		lexxos::kernel::visitTasks(visitTaskForTable, &lookup);
		return lookup.found;
	}

} // namespace

lexxos::program::ProgramDescriptor TasksProgram::descriptor() {
	return {.name = "tasks", .help = "show registered kernel tasks", .handler = run, .legacy = false, .enabled = true};
}

int TasksProgram::run(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	lexxos::apps::PrettyTable table{taskColumns, sizeof(taskColumns) / sizeof(taskColumns[0])};
	table.print(context, taskTableRow, nullptr);
	return 0;
}

} // namespace lexxos::apps::system_programs
