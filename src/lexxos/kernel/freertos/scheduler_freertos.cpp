#include <lexxos/kernel/scheduler.h>

#include <algorithm>
#include <array>

#include <FreeRTOS.h>
#include <task.h>

#include <lexxos/kernel/freertos/freertos_scheduler_types.h>

#if defined(LEXXOS_PLATFORM_PICO) && defined(PICO_RP2350) && PICO_RP2350
namespace lexxos::platform::pico::detail {
void installFreeRtosExceptionHandlers();
}
#endif

namespace {

using lexxos::kernel::freertos::detail::FreeRtosTask;

constexpr std::size_t maxTasks = 8;
std::array<FreeRtosTask, maxTasks> g_tasks{};

configSTACK_DEPTH_TYPE stackSizeToDepth(std::uint32_t stackSizeBytes) {
	auto const depth = stackSizeBytes / static_cast<std::uint32_t>(sizeof(StackType_t));
	return static_cast<configSTACK_DEPTH_TYPE>(std::max<std::uint32_t>(depth, configMINIMAL_STACK_SIZE));
}

std::uint32_t stackDepthToBytes(configSTACK_DEPTH_TYPE stackDepth) {
	return static_cast<std::uint32_t>(stackDepth) * static_cast<std::uint32_t>(sizeof(StackType_t));
}

TickType_t millisecondsToTicks(std::uint32_t milliseconds) {
	auto const ticks = pdMS_TO_TICKS(milliseconds);
	if (ticks == 0) {
		return 1;
	}

	return ticks;
}

UBaseType_t coreAffinityMask(int coreAffinity) {
#if defined(configUSE_CORE_AFFINITY) && configUSE_CORE_AFFINITY && defined(configNUMBER_OF_CORES) && (configNUMBER_OF_CORES > 1)
	if (coreAffinity >= 0 && coreAffinity < configNUMBER_OF_CORES) {
		return static_cast<UBaseType_t>(1U << static_cast<unsigned>(coreAffinity));
	}
#else
	(void) coreAffinity;
#endif
	return 0;
}

int effectiveCoreAffinity(int coreAffinity) {
#if defined(configUSE_CORE_AFFINITY) && configUSE_CORE_AFFINITY && defined(configNUMBER_OF_CORES) && (configNUMBER_OF_CORES > 1)
	if (coreAffinity >= 0 && coreAffinity < configNUMBER_OF_CORES) {
		return coreAffinity;
	}
	return 0;
#else
	return coreAffinity;
#endif
}

void taskTrampoline(void* parameter) {
	auto& task = *static_cast<FreeRtosTask*>(parameter);
	task.snapshot.started = true;

	lexxos::kernel::schedulerBackend().writeConsole("FreeRTOS task start: ");
	lexxos::kernel::schedulerBackend().writeConsole(task.snapshot.name != nullptr ? task.snapshot.name : "<unnamed>");
	lexxos::kernel::schedulerBackend().writeConsole("\r\n");

	task.entry(task.userData);
	task.snapshot.started = false;
	task.handle = nullptr;
	task.entry = nullptr;
	task.userData = nullptr;
	task.allocated = false;
	vTaskDelete(nullptr);
}

FreeRtosTask* allocateTaskSlot() {
	for (auto& task : g_tasks) {
		if (!task.allocated) {
			task.allocated = true;
			return &task;
		}
	}
	return nullptr;
}

} // namespace

namespace lexxos::kernel {

bool createTask(TaskEntry entry, void* userData, TaskOptions options) {
	if (entry == nullptr) {
		return false;
	}

	auto const stackDepth = stackSizeToDepth(options.stackSizeBytes);
	auto* const taskSlot = allocateTaskSlot();
	if (taskSlot == nullptr) {
		return false;
	}
	auto& task = *taskSlot;
	auto const coreAffinity = effectiveCoreAffinity(options.coreAffinity);

	task.entry = entry;
	task.userData = userData;
	task.snapshot = TaskSnapshot{
	    .name = options.name,
	    .stackSizeBytes = stackDepthToBytes(stackDepth),
	    .priority = options.priority,
	    .started = false,
	    .executionClass = options.executionClass,
	    .coreAffinity = coreAffinity,
	};

	BaseType_t result = pdFAIL;
#if defined(configUSE_CORE_AFFINITY) && configUSE_CORE_AFFINITY && defined(configNUMBER_OF_CORES) && (configNUMBER_OF_CORES > 1)
	auto const affinityMask = coreAffinityMask(coreAffinity);
	if (affinityMask != 0) {
		result = xTaskCreateAffinitySet(taskTrampoline,
		    options.name,
		    stackDepth,
		    &task,
		    static_cast<UBaseType_t>(options.priority),
		    affinityMask,
		    &task.handle);
	} else
#endif
	{
		result = xTaskCreate(taskTrampoline,
		    options.name,
		    stackDepth,
		    &task,
		    static_cast<UBaseType_t>(options.priority),
		    &task.handle);
	}
	if (result != pdPASS) {
		task.allocated = false;
		task.handle = nullptr;
		return false;
	}

	return true;
}

void startScheduler() {
#if defined(LEXXOS_PLATFORM_PICO) && defined(PICO_RP2350) && PICO_RP2350
	schedulerBackend().writeConsole("FreeRTOS scheduler: installing exception handlers\r\n");
	::lexxos::platform::pico::detail::installFreeRtosExceptionHandlers();
#endif

	schedulerBackend().writeConsole("FreeRTOS scheduler: starting\r\n");
	vTaskStartScheduler();
	schedulerBackend().writeConsole("FreeRTOS scheduler stopped\r\n");
}

void sleepMs(std::uint32_t milliseconds) {
	if (milliseconds == 0) {
		taskYIELD();
		return;
	}

	vTaskDelay(millisecondsToTicks(milliseconds));
}

void yield() {
	taskYIELD();
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

		auto snapshot = task.snapshot;
		if (task.handle != nullptr) {
			snapshot.stackFreeBytes = static_cast<std::uint32_t>(uxTaskGetStackHighWaterMark(task.handle))
			    * static_cast<std::uint32_t>(sizeof(StackType_t));
		}
		visitor(snapshot, userData);
	}
}

MemorySnapshot memorySnapshot() {
	return MemorySnapshot{
	    .available = true,
	    .freeHeapBytes = static_cast<std::uint32_t>(xPortGetFreeHeapSize()),
	    .minimumEverFreeHeapBytes = static_cast<std::uint32_t>(xPortGetMinimumEverFreeHeapSize()),
	};
}

} // namespace lexxos::kernel
