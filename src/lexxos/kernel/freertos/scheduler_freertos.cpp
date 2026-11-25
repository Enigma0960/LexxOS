#include <array>
#include <limits>

#include <FreeRTOS.h>
#include <task.h>

#include <lexxos/kernel/freertos/freertos_scheduler_types.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/kernel/task_policy.h>

#if defined(LEXXOS_PLATFORM_PICO) && defined(PICO_RP2350) && PICO_RP2350
namespace lexxos::platform::pico::detail {

void installFreeRtosExceptionHandlers();

} // namespace lexxos::platform::pico::detail
#endif

#if defined(LEXXOS_PLATFORM_RK3506)
extern "C" std::uint64_t rk3506_timer_counter();
extern "C" std::uint32_t rk3506_timer_counter_frequency();
#endif

namespace {

using lexxos::kernel::freertos::detail::FreeRtosTask;

// Only suspend after scheduler start; boot creates tasks before the port is running.
class RegistryGuard {
public:
	RegistryGuard()
	    : _active(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
		if (_active) {
			vTaskSuspendAll();
		}
	}

	~RegistryGuard() {
		if (_active) {
			(void) xTaskResumeAll();
		}
	}

	RegistryGuard(RegistryGuard const&) = delete;
	RegistryGuard& operator=(RegistryGuard const&) = delete;

private:
	bool _active;
};

constexpr std::size_t maxTasks = 8;
std::array<FreeRtosTask, maxTasks> g_tasks{};

bool stackSizeToDepth(std::uint32_t stackSizeBytes, configSTACK_DEPTH_TYPE& result) {
	std::uint32_t depth{};
	if (!lexxos::kernel::taskStackDepthFromBytes(
	        stackSizeBytes,
	        sizeof(StackType_t),
	        configMINIMAL_STACK_SIZE,
	        std::numeric_limits<configSTACK_DEPTH_TYPE>::max(),
	        depth)) {
		return false;
	}

	result = static_cast<configSTACK_DEPTH_TYPE>(depth);
	return true;
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

[[maybe_unused]] UBaseType_t coreAffinityMask(int coreAffinity) {
#if defined(configUSE_CORE_AFFINITY) && configUSE_CORE_AFFINITY && defined(configNUMBER_OF_CORES) && (configNUMBER_OF_CORES > 1)
	if (coreAffinity >= 0 && coreAffinity < configNUMBER_OF_CORES) {
		return static_cast<UBaseType_t>(1U << static_cast<unsigned>(coreAffinity));
	}
#else
	(void) coreAffinity;
#endif
	return 0;
}

constexpr int supportedCoreCount() {
#if defined(configNUMBER_OF_CORES) && (configNUMBER_OF_CORES > 1)
	return configNUMBER_OF_CORES;
#else
	return 1;
#endif
}

void taskTrampoline(void* parameter) {
	auto& task = *static_cast<FreeRtosTask*>(parameter);
	{
		RegistryGuard guard;
		task.snapshot.started = true;
	}

#if 0 // Disable debug write
	char line[96]{};
	std::size_t used = 0;
	for (auto text : {std::string_view{"FreeRTOS task start: "}, std::string_view{task.snapshot.name ? task.snapshot.name : "<unnamed>"}, std::string_view{"\r\n"}}) {
		for (char c : text) {
			if (used < sizeof(line)) {
				line[used++] = c;
			}
		}
	}
	lexxos::kernel::schedulerBackend().writeConsole({line, used});
#endif

	task.entry(task.userData);
	{
		RegistryGuard guard;
		task.snapshot.started = false;
		task.retiring = true;
	}
	// Do not publish a free slot before the final instruction of this task.
	// A later task-context reaper deletes only tasks confirmed suspended.
	for (;;) {
		vTaskSuspend(nullptr);
	}
}

// Task context only. The old trampoline can no longer access its slot
// once FreeRTOS reports eSuspended. Delete outside the registry guard.
std::size_t reapRetiredTasks() {
	std::array<TaskHandle_t, maxTasks> retired{};
	std::size_t count = 0;
	{
		RegistryGuard guard;
		for (auto& task : g_tasks) {
			if (!task.allocated || !task.retiring || task.handle == nullptr || eTaskGetState(task.handle) != eSuspended) {
				continue;
			}

			retired[count++] = task.handle;
			task = FreeRtosTask{};
		}
	}

	for (std::size_t i = 0; i < count; ++i) {
		vTaskDelete(retired[i]);
	}

	return count;
}

FreeRtosTask* allocateTaskSlot() {
	for (auto& task : g_tasks) {
		if (!task.allocated) {
			task = FreeRtosTask{};
			task.allocated = true;
			return &task;
		}
	}

	return nullptr;
}

} // namespace

namespace lexxos::kernel {

bool createTask(TaskEntry entry, void* userData, TaskOptions options) {
	if (entry == nullptr
	    || !validTaskPriority(options.priority, configMAX_PRIORITIES)
	    || !validTaskCoreAffinity(options.coreAffinity, supportedCoreCount())) {
		return false;
	}

	configSTACK_DEPTH_TYPE stackDepth{};
	if (!stackSizeToDepth(options.stackSizeBytes, stackDepth)) {
		return false;
	}

	reapRetiredTasks();
	RegistryGuard guard;
	auto* const taskSlot = allocateTaskSlot();
	if (taskSlot == nullptr) {
		return false;
	}

	auto& task = *taskSlot;
	auto const coreAffinity = options.coreAffinity;

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
		task = FreeRtosTask{};
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
#if defined(LEXXOS_PLATFORM_RK3506)
	// LCD initialization needs delays before vTaskStartScheduler().
	// Generic Virtual Counter is already running, but FreeRTOS tick
	// and vTaskDelay() are not available yet.
	if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
		if (milliseconds != 0U) {
			auto const frequency = rk3506_timer_counter_frequency();
			auto const duration =
			    (static_cast<std::uint64_t>(frequency) * milliseconds) / 1000U;
			auto const started = rk3506_timer_counter();
			while (rk3506_timer_counter() - started < duration) {
				asm volatile("nop");
			}
		}

		return;
	}
#endif

	if (milliseconds == 0) {
		taskYIELD();
		return;
	}

	vTaskDelay(millisecondsToTicks(milliseconds));
}

void yield() {
	taskYIELD();
}

std::size_t reapFinishedTasks() {
	return reapRetiredTasks();
}

std::size_t taskCount() {
	RegistryGuard guard;
	std::size_t count = 0;
	for (auto const& task : g_tasks) {
		if (task.allocated && !task.retiring) {
			++count;
		}
	}

	return count;
}

void visitTasks(TaskVisitor visitor, void* userData) {
	if (visitor == nullptr) {
		return;
	}

	std::array<TaskSnapshot, maxTasks> snapshots{};
	std::size_t count = 0;
	{
		RegistryGuard guard;
		for (auto const& task : g_tasks) {
			if (!task.allocated || task.retiring) {
				continue;
			}

			auto& snapshot = snapshots[count++];
			snapshot = task.snapshot;
			if (task.handle != nullptr) {
				snapshot.stackFreeBytes = static_cast<std::uint32_t>(uxTaskGetStackHighWaterMark(task.handle))
				    * static_cast<std::uint32_t>(sizeof(StackType_t));
			}
		}
	}
	for (std::size_t i = 0; i < count; ++i) {
		visitor(snapshots[i], userData);
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
