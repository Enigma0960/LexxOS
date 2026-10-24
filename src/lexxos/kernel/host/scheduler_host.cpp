#include <lexxos/kernel/scheduler.h>

#include <array>
#include <chrono>
#include <thread>

#include <lexxos/kernel/host/host_scheduler_types.h>

namespace {

using lexxos::kernel::host::detail::HostTask;

constexpr std::size_t maxTasks = 8;
std::array<HostTask, maxTasks> g_tasks{};
bool g_schedulerStarted{false};

void taskTrampoline(HostTask& task) {
	task.snapshot.started = true;
	task.entry(task.userData);
	task.snapshot.started = false;
	task.entry = nullptr;
	task.userData = nullptr;
	task.allocated = false;
}

HostTask* allocateTaskSlot() {
	for (auto& task : g_tasks) {
		if (!task.allocated) {
			task.allocated = true;
			return &task;
		}
	}
	return nullptr;
}

void launchTaskThread(HostTask& task, bool detached) {
	task.thread = std::thread([&task]() {
		taskTrampoline(task);
	});
	if (detached) {
		task.thread.detach();
	}
}

} // namespace

namespace lexxos::kernel {

bool createTask(TaskEntry entry, void* userData, TaskOptions options) {
	if (entry == nullptr) {
		return false;
	}

	auto* const taskSlot = allocateTaskSlot();
	if (taskSlot == nullptr) {
		return false;
	}

	auto& task = *taskSlot;
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

	if (g_schedulerStarted) {
		launchTaskThread(task, true);
	}
	return true;
}

void startScheduler() {
	g_schedulerStarted = true;
	for (auto& task : g_tasks) {
		if (!task.allocated || task.thread.joinable()) {
			continue;
		}
		launchTaskThread(task, false);
	}

	for (auto& task : g_tasks) {
		if (task.thread.joinable()) {
			task.thread.join();
		}
	}
}

void sleepMs(std::uint32_t milliseconds) {
	std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

void yield() {
	std::this_thread::yield();
}

std::size_t taskCount() {
	std::size_t count = 0;
	for (auto const& task : g_tasks) {
		if (task.allocated) {
			++count;
		}
	}

	return count;
}

void visitTasks(TaskVisitor visitor, void* userData) {
	if (visitor == nullptr) {
		return;
	}

	for (auto const& task : g_tasks) {
		if (!task.allocated) {
			continue;
		}
		visitor(task.snapshot, userData);
	}
}

MemorySnapshot memorySnapshot() {
	return MemorySnapshot{};
}

} // namespace lexxos::kernel
