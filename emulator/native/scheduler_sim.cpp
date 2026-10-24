#include <lexxos/kernel/scheduler.h>

#include <array>
#include <mutex>
#include <thread>

#include <sim_scheduler_types.h>

namespace {

using lexxos::emulator::detail::SimTask;

constexpr std::size_t maxTasks = 64;
std::array<SimTask, maxTasks> g_tasks{};
std::size_t g_taskCount{0};
std::mutex g_taskLock;
bool g_schedulerStarted{false};

void startTask(SimTask& task) {
	if (task.snapshot.started) {
		return;
	}
	task.snapshot.started = true;
	task.thread = std::thread([&task]() {
		task.entry(task.userData);
	});
}

} // namespace

namespace lexxos::kernel {

bool createTask(TaskEntry entry, void* userData, TaskOptions options) {
	std::lock_guard lock{g_taskLock};
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
	};

	++g_taskCount;
	if (g_schedulerStarted) {
		startTask(task);
	}
	return true;
}

void startScheduler() {
	{
		std::lock_guard lock{g_taskLock};
		g_schedulerStarted = true;
		for (std::size_t index = 0; index < g_taskCount; ++index) {
			startTask(g_tasks[index]);
		}
	}

	std::size_t joinedTasks = 0;
	while (true) {
		SimTask* task = nullptr;
		{
			std::lock_guard lock{g_taskLock};
			if (joinedTasks >= g_taskCount) {
				break;
			}
			task = &g_tasks[joinedTasks++];
		}
		if (task->thread.joinable()) {
			task->thread.join();
		}
	}
}

void sleepMs(std::uint32_t milliseconds) {
	schedulerBackend().sleepMs(milliseconds);
}

void yield() {
	std::this_thread::yield();
}

std::size_t taskCount() {
	std::lock_guard lock{g_taskLock};
	return g_taskCount;
}

void visitTasks(TaskVisitor visitor, void* userData) {
	if (visitor == nullptr) {
		return;
	}

	std::lock_guard lock{g_taskLock};
	for (std::size_t index = 0; index < g_taskCount; ++index) {
		visitor(g_tasks[index].snapshot, userData);
	}
}

MemorySnapshot memorySnapshot() {
	return MemorySnapshot{};
}

} // namespace lexxos::kernel
