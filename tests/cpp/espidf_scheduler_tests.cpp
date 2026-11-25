#include <array>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>

#include <freertos/semphr.h>
#include <freertos/task.h>
#include <lexxos/kernel/scheduler.h>

namespace {

void expect(bool ok, char const* message) {
	if (!ok) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}

struct SuspendedTask {};

struct FakeTask {
	eTaskState state{eReady};
	void (*entry)(void*){};
	void* argument{};
};

std::array<FakeTask, 64> tasks{};
unsigned creates{}, deletes{}, stackBytes{}, priority{}, yields{};
int affinity{-2};
bool failCreate{}, preemptBeforeSuspend{}, runOnCreate{};
thread_local unsigned lockDepth{};
thread_local FakeTask* running{};
std::thread immediateTask;
std::atomic_bool enteredTrampoline{};

void runTask(FakeTask& task) {
	running = &task;
	task.state = eRunning;
	try {
		task.entry(task.argument);
		expect(false, "trampoline returned");
	} catch (SuspendedTask const&) {
	}

	running = nullptr;
}

} // namespace

SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t* storage) {
	return storage;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t mutex, TickType_t) {
	expect(lockDepth == 0, "recursive registry lock");
	mutex->lock();
	++lockDepth;
	return pdTRUE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t mutex) {
	expect(lockDepth == 1, "unbalanced registry lock");
	--lockDepth;
	mutex->unlock();
	return pdTRUE;
}

void testYield() {
	++yields;
}

void vTaskDelay(TickType_t ticks) {
	expect(ticks != 0, "zero delay");
}

std::size_t xPortGetFreeHeapSize() {
	return 100;
}

std::size_t xPortGetMinimumEverFreeHeapSize() {
	return 80;
}

eTaskState eTaskGetState(TaskHandle_t handle) {
	expect(lockDepth == 1, "state read without registry lock");
	return static_cast<FakeTask*>(handle)->state;
}

void vTaskDelete(TaskHandle_t handle) {
	expect(handle && lockDepth == 1, "task delete races registry");
	auto& task = *static_cast<FakeTask*>(handle);
	expect(task.state == eSuspended, "unfinished task deleted");
	task.state = eDeleted;
	++deletes;
}

void vTaskSuspend(TaskHandle_t handle) {
	expect(!handle && running && lockDepth == 0, "invalid self-suspend");
	if (preemptBeforeSuspend) {
		preemptBeforeSuspend = false;
		expect(lexxos::kernel::taskCount() == 0, "retiring task reported active");
		expect(lexxos::kernel::reapFinishedTasks() == 0, "retired before suspension");
		expect(lexxos::kernel::createTask([](void*) {}, nullptr, {}), "competing create failed");
		expect(running->argument != tasks[creates - 1].argument, "running slot reused");
	}

	running->state = eSuspended;
	throw SuspendedTask{};
}

UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t handle) {
	expect(handle && lockDepth == 1, "watermark read races deletion");
	return 333;
}

BaseType_t xTaskCreate(void (*entry)(void*), char const*, std::uint32_t bytes, void* argument,
    UBaseType_t taskPriority, TaskHandle_t* handle) {
	expect(lockDepth == 1, "creator must publish handle under registry lock");
	if (failCreate) {
		return pdFAIL;
	}

	auto& task = tasks[creates++];
	task = {eReady, entry, argument};
	stackBytes = bytes;
	priority = taskPriority;
	if (runOnCreate) {
		enteredTrampoline = false;
		immediateTask = std::thread([&task] { enteredTrampoline = true; runTask(task); });
		while (!enteredTrampoline) {
			std::this_thread::yield();
		}
	}

	*handle = &task;
	return pdPASS;
}

BaseType_t xTaskCreatePinnedToCore(void (*entry)(void*), char const* name, std::uint32_t bytes,
    void* argument, UBaseType_t taskPriority, TaskHandle_t* handle, BaseType_t core) {
	affinity = core;
	return xTaskCreate(entry, name, bytes, argument, taskPriority, handle);
}

int main() {
	namespace kernel = lexxos::kernel;
	expect(!kernel::createTask(nullptr, nullptr, {}), "null entry accepted");
	expect(!kernel::createTask([](void*) {}, nullptr, {.stackSizeBytes = 0}), "zero stack accepted");
	expect(!kernel::createTask([](void*) {}, nullptr, {.priority = configMAX_PRIORITIES}), "bad priority accepted");
	expect(!kernel::createTask([](void*) {}, nullptr, {.coreAffinity = -2}), "bad affinity accepted");
	expect(!kernel::createTask([](void*) {}, nullptr, {.coreAffinity = 2}), "missing core accepted");
#if CONFIG_FREERTOS_UNICORE
	expect(!kernel::createTask([](void*) {}, nullptr, {.coreAffinity = 1}), "second core on unicore");
#endif
	expect(kernel::createTask([](void*) {}, nullptr, {.stackSizeBytes = 4097, .priority = 300}), "create failed");
	expect(stackBytes == 4097 && priority == 300, "stack bytes or priority truncated");
#if !CONFIG_FREERTOS_UNICORE
	expect(affinity == 0, "Standard default affinity changed");
#endif
	unsigned visits = 0;
	kernel::visitTasks([](kernel::TaskSnapshot const& snapshot, void* data) {
		expect(lockDepth == 0, "callback inside registry lock");
		expect(snapshot.stackSizeBytes == 4097 && snapshot.stackFreeBytes == 333, "snapshot units incorrect");
		expect(kernel::taskCount() == 1, "visitor reentry failed");
		++*static_cast<unsigned*>(data);
	},
	    &visits);
	expect(visits == 1, "snapshot omitted task");
	failCreate = true;
	expect(!kernel::createTask([](void*) {}, nullptr, {}) && kernel::taskCount() == 1, "failure leaked slot");
	failCreate = false;
	preemptBeforeSuspend = true;
	runTask(tasks[0]);
	expect(deletes == 0 && kernel::taskCount() == 1, "premature retirement");
	expect(kernel::reapFinishedTasks() == 1 && deletes == 1, "reaper failed");
	expect(kernel::reapFinishedTasks() == 0, "double retirement");
	runTask(tasks[1]);
	expect(kernel::reapFinishedTasks() == 1, "second retirement failed");
	unsigned entries = 0;
	runOnCreate = true;
	for (unsigned i = 0; i < 20; ++i) {
		expect(kernel::createTask([](void* data) { ++*static_cast<unsigned*>(data); }, &entries, {.stackSizeBytes = 1}), "immediate task create failed");
		immediateTask.join();
		expect(stackBytes == configMINIMAL_STACK_SIZE && kernel::taskCount() == 0, "minimum stack or completion incorrect");
		expect(kernel::reapFinishedTasks() == 1, "immediate task lost handle");
	}

	expect(entries == 20, "immediate task did not execute exactly once");
	runOnCreate = false;
	std::array<std::thread, 8> creators;
	for (auto& creator : creators) {
		creator = std::thread([] {
			expect(kernel::createTask([](void*) {}, nullptr, {}), "concurrent slot unavailable");
		});
	}

	for (auto& creator : creators) {
		creator.join();
	}

	expect(!kernel::createTask([](void*) {}, nullptr, {}) && kernel::taskCount() == 8, "registry capacity exceeded");
	kernel::sleepMs(0);
	kernel::sleepMs(1);
	kernel::yield();
	expect(yields == 2 && kernel::memorySnapshot().minimumEverFreeHeapBytes == 80, "scheduler helpers");
}
