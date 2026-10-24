#include <lexxos/kernel/scheduler.h>

#include <array>

#include <lexxos/kernel/pico/bootstrap_scheduler_types.h>

namespace {

using lexxos::kernel::pico::detail::BootstrapTask;

constexpr std::size_t maxTasks = 8;
std::array<BootstrapTask, maxTasks> g_tasks{};
std::size_t g_taskCount{0};

} // namespace

namespace lexxos::kernel {

bool createTask(TaskEntry entry, void* userData, TaskOptions options) {
	if (entry == nullptr || g_taskCount >= g_tasks.size()) {
		return false;
	}

	auto& task = g_tasks[g_taskCount];
	task.entry = entry;
	task.userData = userData;
	task.snapshot = TaskSnapshot{
	    .name = options.name,
	    .stackSizeBytes = options.stackSizeBytes,
	    .priority = options.priority,
	    .started = false,
	    .executionClass = options.executionClass,
	    .coreAffinity = options.coreAffinity,
	};

	++g_taskCount;
	return true;
}

void startScheduler() {
	if (g_taskCount == 0) {
		return;
	}

	schedulerBackend().writeConsole("bootstrap scheduler: running first task only\r\n");
	g_tasks[0].snapshot.started = true;
	g_tasks[0].entry(g_tasks[0].userData);
}

void sleepMs(std::uint32_t milliseconds) {
	schedulerBackend().sleepMs(milliseconds);
}

void yield() {
	schedulerBackend().yield();
}

std::size_t taskCount() {
	return g_taskCount;
}

void visitTasks(TaskVisitor visitor, void* userData) {
	if (visitor == nullptr) {
		return;
	}

	for (std::size_t index = 0; index < g_taskCount; ++index) {
		visitor(g_tasks[index].snapshot, userData);
	}
}

MemorySnapshot memorySnapshot() {
	return MemorySnapshot{};
}

} // namespace lexxos::kernel
