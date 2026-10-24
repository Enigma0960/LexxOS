#include <lexxos/apps/core/top_program.h>

#include <cstdint>

#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/common/pretty_table.h>
#include <lexxos/feature.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/app_context.h>
#include <lexxos/program/program_supervisor.h>

namespace lexxos::apps::system_programs {
namespace {

	constexpr std::uint32_t defaultIntervalMs = 1000;
	constexpr std::uint32_t minimumIntervalMs = 100;
	constexpr std::uint32_t maximumIntervalMs = 60000;

	constexpr lexxos::program::ProgramArgumentSpec topArguments[] = {
	    {
         .name = "interval",
         .shortName = 'i',
         .valueName = "ms",
         .help = "refresh interval in milliseconds, clamped to 100..60000",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "1000",
	     },
	    {
         .name = "count",
         .shortName = 'n',
         .valueName = "samples",
         .help = "number of refreshes; 0 means run until cancelled",
         .kind = lexxos::program::ProgramArgumentKind::Option,
         .valueType = lexxos::program::ProgramArgumentValueType::Unsigned,
         .required = false,
         .defaultValue = "0",
	     },
	    {
         .name = "no-clear",
         .help = "append snapshots instead of clearing the terminal before every refresh",
         .kind = lexxos::program::ProgramArgumentKind::Flag,
         .required = false,
	     },
	    {
         .name = "no-jobs",
         .help = "hide ProgramSupervisor job memory table",
         .kind = lexxos::program::ProgramArgumentKind::Flag,
         .required = false,
	     },
	};

	struct TaskTotals {
		std::uint32_t count{0};
		std::uint32_t started{0};
		std::uint32_t stackBytes{0};
		std::uint32_t stackFreeBytes{0};
	};

	struct JobMemoryTotals {
		std::uint32_t count{0};
		std::uint32_t active{0};
		std::uint32_t internalReservedBytes{0};
		std::uint32_t internalQuotaBytes{0};
		std::uint32_t externalReservedBytes{0};
		std::uint32_t externalQuotaBytes{0};
	};

	struct TopSettings {
		std::uint32_t intervalMs{defaultIntervalMs};
		std::uint32_t count{0};
		bool clearScreen{true};
		bool showJobs{true};
	};

	constexpr lexxos::apps::PrettyTableColumn taskColumns[] = {
	    {    .name = "name",  .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 4, .maxWidth = 16},
	    {.name = "priority", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 8,  .maxWidth = 8},
	    {   .name = "class",  .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 5, .maxWidth = 10},
	    {    .name = "core", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 4,  .maxWidth = 4},
	    { .name = "started",  .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 7,  .maxWidth = 7},
	    {   .name = "stack", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 5, .maxWidth = 10},
	    {    .name = "used", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 4, .maxWidth = 10},
	    {    .name = "free", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 4, .maxWidth = 10},
	    {     .name = "use", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 3,  .maxWidth = 4},
	};

	constexpr lexxos::apps::PrettyTableColumn jobColumns[] = {
	    {      .name = "id", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 2,  .maxWidth = 4},
	    {   .name = "state",  .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 5, .maxWidth = 14},
	    { .name = "program",  .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 7, .maxWidth = 14},
	    {   .name = "iface",  .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 5,  .maxWidth = 9},
	    {   .name = "stack", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 5, .maxWidth = 10},
	    {    .name = "used", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 4, .maxWidth = 10},
	    {    .name = "free", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 4, .maxWidth = 10},
	    {.name = "priority", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 8,  .maxWidth = 8},
	    {  .name = "memory",  .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 6, .maxWidth = 13},
	    {     .name = "int", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 3, .maxWidth = 11},
	    {     .name = "ext", .align = lexxos::apps::PrettyTableAlign::Right, .minWidth = 3, .maxWidth = 11},
	    {   .name = "arena",  .align = lexxos::apps::PrettyTableAlign::Left, .minWidth = 5, .maxWidth = 12},
	};

	bool isActiveProcess(lexxos::program::ProgramProcessState state) {
		return state == lexxos::program::ProgramProcessState::Starting
		    || state == lexxos::program::ProgramProcessState::Running
		    || state == lexxos::program::ProgramProcessState::StopRequested;
	}

	void writePercent(lexxos::os::AppContext& context, std::uint32_t value, std::uint32_t total) {
		if (total == 0) {
			context.write("n/a");
			return;
		}

		auto const percent = static_cast<std::uint32_t>((static_cast<std::uint64_t>(value) * 100ULL) / total);
		writeUnsigned(context, percent);
		context.write("%");
	}

	void writeBytes(lexxos::os::AppContext& context, std::uint32_t value) {
		writeUnsigned(context, value);
		context.write("B");
	}

	void appendBytes(lexxos::apps::PrettyTableRow& row, std::size_t column, std::uint32_t value) {
		row.appendUnsigned(column, value);
		row.append(column, "B");
	}

	void appendPercent(
	    lexxos::apps::PrettyTableRow& row,
	    std::size_t column,
	    std::uint32_t value,
	    std::uint32_t total) {
		if (total == 0) {
			row.set(column, "n/a");
			return;
		}
		auto const percent = static_cast<std::uint32_t>((static_cast<std::uint64_t>(value) * 100ULL) / total);
		row.appendUnsigned(column, percent);
		row.append(column, "%");
	}

	void collectTaskTotals(TaskTotals& totals) {
		lexxos::kernel::visitTasks(
		    [](lexxos::kernel::TaskSnapshot const& task, void* userData) {
			    auto& taskTotals = *static_cast<TaskTotals*>(userData);
			    ++taskTotals.count;
			    if (task.started) {
				    ++taskTotals.started;
			    }
			    taskTotals.stackBytes += task.stackSizeBytes;
			    taskTotals.stackFreeBytes += task.stackFreeBytes;
		    },
		    &totals);
	}

	void collectJobMemoryTotals(JobMemoryTotals& totals) {
		bool any = false;
		lexxos::program::supervisor().visitProcesses(
		    [](lexxos::program::ProgramProcessSnapshot const& process, void* userData) {
			    auto& jobTotals = *static_cast<JobMemoryTotals*>(userData);
			    ++jobTotals.count;
			    if (isActiveProcess(process.state)) {
				    ++jobTotals.active;
			    }
			    jobTotals.internalReservedBytes += process.internalMemoryReservedBytes;
			    jobTotals.internalQuotaBytes += process.internalMemoryQuotaBytes;
			    jobTotals.externalReservedBytes += process.externalMemoryReservedBytes;
			    jobTotals.externalQuotaBytes += process.externalMemoryQuotaBytes;
		    },
		    &totals,
		    any);
	}

	void writeHeader(lexxos::os::AppContext& context, std::uint32_t sample, TopSettings const& settings) {
		context.write("LexxOS top uptime-ms=");
		writeUnsigned(context, uptimeMs(context));
		context.write(" sample=");
		writeUnsigned(context, sample);
		context.write(" interval-ms=");
		writeUnsigned(context, settings.intervalMs);
		context.writeLine("  cancel: Ctrl+C/stop");
	}

	void writeMemorySummary(lexxos::os::AppContext& context, TaskTotals const& tasks, JobMemoryTotals const& jobs) {
		context.writeLine("MEMORY");

		auto const memory = lexxos::kernel::memorySnapshot();
		context.write("internal heap: ");
		if (!memory.available) {
			context.writeLine("not available on this backend");
		} else {
			context.write("free=");
			writeBytes(context, memory.freeHeapBytes);
			context.write(" lowest-free=");
			writeBytes(context, memory.minimumEverFreeHeapBytes);
			if (memory.freeHeapBytes >= memory.minimumEverFreeHeapBytes) {
				context.write(" recovered-margin=");
				writeBytes(context, memory.freeHeapBytes - memory.minimumEverFreeHeapBytes);
			}
			context.writeLine("");
		}

		auto const stackUsedBytes = tasks.stackBytes >= tasks.stackFreeBytes ? tasks.stackBytes - tasks.stackFreeBytes : 0U;
		context.write("task stacks: total=");
		writeBytes(context, tasks.stackBytes);
		context.write(" used=");
		writeBytes(context, stackUsedBytes);
		context.write(" free=");
		writeBytes(context, tasks.stackFreeBytes);
		context.write(" use=");
		writePercent(context, stackUsedBytes, tasks.stackBytes);
		context.writeLine("");

		context.write("program arena internal: reserved=");
		writeBytes(context, jobs.internalReservedBytes);
		context.write(" quota=");
		writeBytes(context, jobs.internalQuotaBytes);
		context.writeLine("");

		auto const& board = activeBoard(context);
		context.write("psram: build=");
#if FEATURE_ENABLED(PSRAM)
		context.write("enabled");
#else
		context.write("disabled");
#endif
		context.write(" wiring=");
		if (board.psram.isQuadSpiLike()) {
			context.write("quad-spi-like");
		} else if (board.psram.spi.isValid()) {
			context.write("spi");
		} else {
			context.write("not-configured");
		}
		context.write(" reserved=");
		writeBytes(context, jobs.externalReservedBytes);
		context.write(" quota=");
		writeBytes(context, jobs.externalQuotaBytes);
		context.writeLine(" global-free=n/a");
	}

	struct TaskLookup {
		std::size_t target{0};
		std::size_t current{0};
		lexxos::apps::PrettyTableRow* row{nullptr};
		bool found{false};
	};

	void visitTaskForTable(lexxos::kernel::TaskSnapshot const& task, void* userData) {
		auto& lookup = *static_cast<TaskLookup*>(userData);
		if (lookup.current == lookup.target && lookup.row != nullptr) {
			auto const stackUsedBytes = task.stackSizeBytes >= task.stackFreeBytes ? task.stackSizeBytes - task.stackFreeBytes : 0U;
			lookup.row->set(0, task.name);
			lookup.row->setUnsigned(1, task.priority);
			lookup.row->set(2, lexxos::kernel::toString(task.executionClass));
			if (task.coreAffinity < 0) {
				lookup.row->set(3, "any");
			} else {
				lookup.row->setUnsigned(3, static_cast<std::uint32_t>(task.coreAffinity));
			}
			lookup.row->set(4, task.started ? "yes" : "no");
			appendBytes(*lookup.row, 5, task.stackSizeBytes);
			appendBytes(*lookup.row, 6, stackUsedBytes);
			appendBytes(*lookup.row, 7, task.stackFreeBytes);
			appendPercent(*lookup.row, 8, stackUsedBytes, task.stackSizeBytes);
			lookup.found = true;
		}
		++lookup.current;
	}

	bool taskTableRow(std::size_t rowIndex, lexxos::apps::PrettyTableRow& row, void*) {
		TaskLookup lookup{.target = rowIndex, .row = &row};
		lexxos::kernel::visitTasks(visitTaskForTable, &lookup);
		return lookup.found;
	}

	struct JobLookup {
		std::size_t target{0};
		std::size_t current{0};
		lexxos::apps::PrettyTableRow* row{nullptr};
		bool found{false};
	};

	void visitJobForTable(lexxos::program::ProgramProcessSnapshot const& process, void* userData) {
		auto& lookup = *static_cast<JobLookup*>(userData);
		if (lookup.current == lookup.target && lookup.row != nullptr) {
			auto const stackUsedBytes = process.stackSizeBytes >= process.stackFreeBytes ? process.stackSizeBytes - process.stackFreeBytes : 0U;
			lookup.row->setUnsigned(0, process.id);
			lookup.row->set(1, lexxos::program::toString(process.state));
			lookup.row->set(2, process.name);
			lookup.row->set(3, lexxos::program::toString(process.interfaceKind));
			appendBytes(*lookup.row, 4, process.stackSizeBytes);
			appendBytes(*lookup.row, 5, stackUsedBytes);
			appendBytes(*lookup.row, 6, process.stackFreeBytes);
			lookup.row->setUnsigned(7, process.priority);
			lookup.row->set(8, lexxos::program::toString(process.memoryClass));
			appendBytes(*lookup.row, 9, process.internalMemoryReservedBytes);
			lookup.row->append(9, "/");
			appendBytes(*lookup.row, 9, process.internalMemoryQuotaBytes);
			appendBytes(*lookup.row, 10, process.externalMemoryReservedBytes);
			lookup.row->append(10, "/");
			appendBytes(*lookup.row, 10, process.externalMemoryQuotaBytes);
			lookup.row->set(11, lexxos::program::toString(process.memoryArenaStatus));
			lookup.found = true;
		}
		++lookup.current;
	}

	bool jobTableRow(std::size_t rowIndex, lexxos::apps::PrettyTableRow& row, void*) {
		JobLookup lookup{.target = rowIndex, .row = &row};
		bool any = false;
		lexxos::program::supervisor().visitProcesses(visitJobForTable, &lookup, any);
		return lookup.found;
	}

	void writeTaskTable(lexxos::os::AppContext& context, TaskTotals const& totals) {
		context.writeLine("");
		context.write("TASKS total=");
		writeUnsigned(context, totals.count);
		context.write(" started=");
		writeUnsigned(context, totals.started);
		context.writeLine("");
		lexxos::apps::PrettyTable table{taskColumns, sizeof(taskColumns) / sizeof(taskColumns[0])};
		table.print(context, taskTableRow, nullptr);
	}

	void writeJobTable(lexxos::os::AppContext& context, JobMemoryTotals const& totals) {
		context.writeLine("");
		context.write("JOBS total=");
		writeUnsigned(context, totals.count);
		context.write(" active=");
		writeUnsigned(context, totals.active);
		context.writeLine("");

		bool any = false;
		lexxos::program::supervisor().visitProcesses(
		    [](lexxos::program::ProgramProcessSnapshot const&, void*) {},
		    nullptr,
		    any);

		if (!any) {
			context.writeLine("no supervised processes");
			return;
		}
		lexxos::apps::PrettyTable table{jobColumns, sizeof(jobColumns) / sizeof(jobColumns[0])};
		table.setMaxTableWidth(0);
		table.print(context, jobTableRow, nullptr);
	}

	void writeSnapshot(lexxos::os::AppContext& context, std::uint32_t sample, TopSettings const& settings) {
		if (settings.clearScreen) {
			context.write("\x1B[2J\x1B[H");
		}

		TaskTotals tasks{};
		JobMemoryTotals jobs{};
		collectTaskTotals(tasks);
		collectJobMemoryTotals(jobs);

		writeHeader(context, sample, settings);
		writeMemorySummary(context, tasks, jobs);
		writeTaskTable(context, tasks);
		if (settings.showJobs) {
			writeJobTable(context, jobs);
		}
	}

	bool parseSettings(lexxos::os::AppContext& context, lexxos::program::ParsedArguments const& arguments, TopSettings& settings) {
		if (!parseUnsigned32(arguments.valueOr("interval", "1000"), settings.intervalMs)) {
			context.writeErrorLine("top: invalid interval");
			return false;
		}
		if (settings.intervalMs < minimumIntervalMs) {
			settings.intervalMs = minimumIntervalMs;
		}
		if (settings.intervalMs > maximumIntervalMs) {
			settings.intervalMs = maximumIntervalMs;
		}

		if (!parseUnsigned32(arguments.valueOr("count", "0"), settings.count)) {
			context.writeErrorLine("top: invalid count");
			return false;
		}

		settings.clearScreen = !arguments.has("no-clear");
		settings.showJobs = !arguments.has("no-jobs");
		return true;
	}

} // namespace

lexxos::program::ProgramDescriptor TopProgram::descriptor() {
	return {
	    .name = "top",
	    .help = "watch memory, tasks and supervised jobs",
	    .description = "Periodically refreshes a compact memory-first dashboard with internal heap, task stack usage, PSRAM reservation accounting and ProgramSupervisor jobs. It intentionally does not report CPU percent until the scheduler exposes runtime counters.",
	    .usage = "top [--interval <ms>] [--count <samples>] [--no-clear] [--no-jobs]",
	    .arguments = lexxos::apps::argumentList(topArguments),
	    .handler = run,
	    .runtime = {
	                .launchMode = lexxos::program::ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 4096,
	                .priority = 1,
	                .memoryClass = lexxos::program::ProgramMemoryClass::InternalOnly,
	                .externalMemoryQuotaBytes = 0,
	                .internalMemoryQuotaBytes = 0,
	                .cancellable = true,
	                .allowBackground = true,
	                },
	    .legacy = false,
	    .enabled = true,
	};
}

int TopProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const& arguments) {
	TopSettings settings{};
	if (!parseSettings(context, arguments, settings)) {
		return 2;
	}

	std::uint32_t sample = 0;
	while (settings.count == 0 || sample < settings.count) {
		++sample;
		writeSnapshot(context, sample, settings);

		if (settings.count != 0 && sample >= settings.count) {
			break;
		}

		std::uint32_t remainingMs = settings.intervalMs;
		while (remainingMs > 0) {
			if (context.pollCancellationRequest()) {
				context.writeLine("top: canceled");
				return 130;
			}
			auto const chunkMs = remainingMs > 50 ? 50U : remainingMs;
			sleepMs(context, chunkMs);
			remainingMs -= chunkMs;
		}
	}

	return 0;
}

} // namespace lexxos::apps::system_programs
