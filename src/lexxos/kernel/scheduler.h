#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace lexxos::platform {
struct PlatformServices;
}

namespace lexxos::kernel {

using TaskEntry = void (*)(void* userData);

enum class TaskExecutionClass : std::uint8_t {
	System,
	Driver,
	Program,
	Background,
};

[[nodiscard]] constexpr std::string_view toString(TaskExecutionClass executionClass) {
	switch (executionClass) {
		case TaskExecutionClass::System:
			return "system";
		case TaskExecutionClass::Driver:
			return "driver";
		case TaskExecutionClass::Program:
			return "program";
		case TaskExecutionClass::Background:
			return "background";
	}
	return "system";
}

inline constexpr int anyTaskCore = -1;

struct TaskOptions {
	char const* name{"unnamed"};
	std::uint32_t stackSizeBytes{2048};
	std::uint32_t priority{1};
	TaskExecutionClass executionClass{TaskExecutionClass::System};
	int coreAffinity{anyTaskCore};
};

struct TaskSnapshot {
	char const* name{"unnamed"};
	std::uint32_t stackSizeBytes{0};
	std::uint32_t stackFreeBytes{0};
	std::uint32_t priority{0};
	bool started{false};
	TaskExecutionClass executionClass{TaskExecutionClass::System};
	int coreAffinity{anyTaskCore};
};

struct MemorySnapshot {
	bool available{false};
	std::uint32_t freeHeapBytes{0};
	std::uint32_t minimumEverFreeHeapBytes{0};
};

struct SchedulerBackend {
	void (*sleepMs)(std::uint32_t milliseconds){nullptr};
	void (*yield)(){nullptr};
	void (*writeConsole)(std::string_view text){nullptr};
};

using TaskVisitor = void (*)(TaskSnapshot const& task, void* userData);

void bindSchedulerBackend(SchedulerBackend backend);
void bindSchedulerBackend(lexxos::platform::PlatformServices const& platform);
[[nodiscard]] SchedulerBackend const& schedulerBackend();

bool createTask(TaskEntry entry, void* userData, TaskOptions options);
void startScheduler();
void sleepMs(std::uint32_t milliseconds);
void yield();
std::size_t taskCount();
void visitTasks(TaskVisitor visitor, void* userData);
MemorySnapshot memorySnapshot();

} // namespace lexxos::kernel
