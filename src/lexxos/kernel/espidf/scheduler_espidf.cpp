#include <algorithm>
#include <array>
#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <lexxos/kernel/espidf/task_registry_guard.h>
#include <lexxos/kernel/freertos/freertos_scheduler_types.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/kernel/task_policy.h>

namespace {

using lexxos::kernel::espidf::detail::TaskRegistryGuard;
using lexxos::kernel::freertos::detail::FreeRtosTask;

// Mutex допускает блокирующие API FreeRTOS и защищает реестр на обоих ядрах.
StaticSemaphore_t g_registryMutexStorage;
SemaphoreHandle_t const g_registryMutex = xSemaphoreCreateMutexStatic(&g_registryMutexStorage);

constexpr std::size_t maxTasks = 8;
std::array<FreeRtosTask, maxTasks> g_tasks{};

TickType_t millisecondsToTicks(std::uint32_t milliseconds) {
	auto const ticks = pdMS_TO_TICKS(milliseconds);
	if (ticks == 0) {
		return 1;
	}

	return ticks;
}

int effectiveCoreAffinity(int coreAffinity) {
#if CONFIG_FREERTOS_UNICORE
	return coreAffinity;
#else
	if (coreAffinity >= 0 && coreAffinity <= 1) {
		return coreAffinity;
	}

	return 0;
#endif
}

void taskTrampoline(void* parameter) {
	auto& task = *static_cast<FreeRtosTask*>(parameter);
	{
		// Создатель удерживает mutex до публикации handle: задача может стартовать на другом ядре.
		TaskRegistryGuard guard;
		task.snapshot.started = true;
	}

	task.entry(task.userData);
	{
		TaskRegistryGuard guard;
		task.snapshot.started = false;
		task.retiring = true;
	}

	// Слот остаётся занят до подтверждённой остановки trampoline и удаления задачи.
	for (;;) {
		vTaskSuspend(nullptr);
	}
}

std::size_t reapRetiredTasks() {
	TaskRegistryGuard guard;
	std::size_t count = 0;
	for (auto& task : g_tasks) {
		if (!task.allocated || !task.retiring || task.handle == nullptr || eTaskGetState(task.handle) != eSuspended) {
			continue;
		}

		vTaskDelete(task.handle);
		task = FreeRtosTask{};
		++count;
	}

	return count;
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

BaseType_t createEspIdfTask(FreeRtosTask& task, std::uint32_t stackSizeBytes, UBaseType_t priority) {
#if CONFIG_FREERTOS_UNICORE
	return xTaskCreate(
	    taskTrampoline,
	    task.snapshot.name,
	    stackSizeBytes,
	    &task,
	    static_cast<UBaseType_t>(priority),
	    &task.handle);
#else
	auto const affinity = task.snapshot.coreAffinity;
	if (affinity >= 0 && affinity <= 1) {
		return xTaskCreatePinnedToCore(
		    taskTrampoline,
		    task.snapshot.name,
		    stackSizeBytes,
		    &task,
		    static_cast<UBaseType_t>(priority),
		    &task.handle,
		    affinity);
	}

	return xTaskCreate(
	    taskTrampoline,
	    task.snapshot.name,
	    stackSizeBytes,
	    &task,
	    static_cast<UBaseType_t>(priority),
	    &task.handle);
#endif
}

} // namespace

namespace lexxos::kernel::espidf::detail {

TaskRegistryGuard::TaskRegistryGuard() {
	auto const locked = xSemaphoreTake(g_registryMutex, portMAX_DELAY);
	configASSERT(locked == pdTRUE);
	(void) locked;
}

TaskRegistryGuard::~TaskRegistryGuard() {
	(void) xSemaphoreGive(g_registryMutex);
}

} // namespace lexxos::kernel::espidf::detail

namespace lexxos::kernel {

bool createTask(TaskEntry entry, void* userData, TaskOptions options) {
	if (entry == nullptr || options.stackSizeBytes == 0U || !validTaskPriority(options.priority, configMAX_PRIORITIES) || !validTaskCoreAffinity(options.coreAffinity,
#if CONFIG_FREERTOS_UNICORE
	        1
#else
	        2
#endif
	        )) {
		return false;
	}

	// В ESP-IDF размер стека и high-water mark выражаются в байтах, независимо от StackType_t.
	auto const stackSizeBytes = std::max<std::uint32_t>(options.stackSizeBytes, configMINIMAL_STACK_SIZE);
	reapRetiredTasks();
	TaskRegistryGuard guard;
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
	    .stackSizeBytes = stackSizeBytes,
	    .priority = options.priority,
	    .started = false,
	    .executionClass = options.executionClass,
	    .coreAffinity = coreAffinity,
	};

	auto const result = createEspIdfTask(task, stackSizeBytes, options.priority);
	if (result != pdPASS) {
		task = FreeRtosTask{};
		return false;
	}

	return true;
}

void startScheduler() {
	// В ESP-IDF FreeRTOS уже запущен до входа в app_main().
	// После создания задач LexxOS текущая app_main task больше не нужна.
	vTaskDelete(nullptr);
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

std::size_t reapFinishedTasks() {
	return reapRetiredTasks();
}

std::size_t taskCount() {
	TaskRegistryGuard guard;
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
		TaskRegistryGuard guard;
		for (auto const& task : g_tasks) {
			if (!task.allocated || task.retiring) {
				continue;
			}

			auto& snapshot = snapshots[count++];
			snapshot = task.snapshot;
			if (task.handle != nullptr) {
				snapshot.stackFreeBytes = static_cast<std::uint32_t>(uxTaskGetStackHighWaterMark(task.handle));
			}
		}
	}
	// Пользовательский callback может повторно обратиться к реестру.
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
