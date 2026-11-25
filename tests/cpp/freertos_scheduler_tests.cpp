#include <array>
#include <cstdlib>
#include <iostream>

#include <FreeRTOS.h>

#include <lexxos/kernel/scheduler.h>
#include <lexxos/kernel/task_policy.h>

namespace {

unsigned suspended{}, creates{}, depths{}, deletes{}, writes{};
bool failCreate{}, preemptBeforeSuspend{};
void (*lastEntry)(void*){};
void* lastArgument{};
TaskHandle_t runningHandle{};

struct SuspendedTask {};

struct FakeTask {
	TaskHandle_t handle{};
	eTaskState state{eInvalid};
};

std::array<FakeTask, 16> fakeTasks{};

void expect(bool ok, char const* message) {
	if (!ok) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}

FakeTask& fake(TaskHandle_t handle) {
	for (auto& task : fakeTasks) {
		if (task.handle == handle) {
			return task;
		}
	}

	expect(false, "unknown fake task");
	return fakeTasks[0];
}

void runTask() {
	runningHandle = fakeTasks[creates - 1].handle;
	auto entry = lastEntry;
	auto* argument = lastArgument;
	fake(runningHandle).state = eRunning;
	try {
		entry(argument);
		expect(false, "finished task returned from trampoline");
	} catch (SuspendedTask const&) {
		// The mock emulates a self-suspending FreeRTOS task.
	}

	runningHandle = nullptr;
}

} // namespace

int xTaskGetSchedulerState() {
	return taskSCHEDULER_RUNNING;
}

void vTaskSuspendAll() {
	++suspended;
}

int xTaskResumeAll() {
	expect(suspended > 0, "unbalanced resume");
	--suspended;
	return 0;
}

void testYield() {
}

std::size_t xPortGetFreeHeapSize() {
	return 100;
}

std::size_t xPortGetMinimumEverFreeHeapSize() {
	return 80;
}

void vTaskDelete(TaskHandle_t handle) {
	expect(suspended == 0, "delete inside registry guard");
	expect(fake(handle).state == eSuspended, "cannot delete unfinished task");
	fake(handle).state = eDeleted;
	++deletes;
}

void vTaskSuspend(TaskHandle_t handle) {
	expect(handle == nullptr && suspended == 0, "task suspension context");
	expect(runningHandle != nullptr, "self-suspend without running task");
	if (preemptBeforeSuspend) {
		preemptBeforeSuspend = false;
		expect(lexxos::kernel::taskCount() == 0, "retiring task reported active");

		expect(lexxos::kernel::reapFinishedTasks() == 0 && deletes == 0,
		    "maintenance reaped task before it suspended");

		expect(lexxos::kernel::createTask([](void*) {}, nullptr, {.name = "competing"}),
		    "concurrent creation failed");
		expect(deletes == 0, "reaped task before it suspended");
	}

	fake(runningHandle).state = eSuspended;
	throw SuspendedTask{};
}

eTaskState eTaskGetState(TaskHandle_t handle) {
	return fake(handle).state;
}

void vTaskDelay(TickType_t) {
}

void vTaskStartScheduler() {
}

UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t) {
	expect(suspended > 0, "watermark read races task deletion");
	return 3;
}

BaseType_t xTaskCreate(void (*entry)(void*), char const*, std::uint32_t depth, void* argument,
    UBaseType_t, TaskHandle_t* handle) {
	expect(suspended > 0, "new task may run before slot is published");
	if (failCreate) {
		return pdFAIL;
	}

	++creates;
	depths = depth;
	lastEntry = entry;
	lastArgument = argument;
	*handle = reinterpret_cast<TaskHandle_t>(static_cast<std::uintptr_t>(creates));
	fakeTasks[creates - 1] = {*handle, eReady};
	return pdPASS;
}

namespace lexxos::kernel {

SchedulerBackend const& schedulerBackend() {
	static SchedulerBackend backend{.writeConsole = [](std::string_view text) {
		expect(text.ends_with("\r\n"), "task start message split into fragments");
		++writes;
	}};
	return backend;
}

} // namespace lexxos::kernel

int main() {
	namespace kernel = lexxos::kernel;
	static_assert(kernel::validTaskCoreAffinity(kernel::anyTaskCore, 1));
	static_assert(kernel::validTaskCoreAffinity(0, 1));
	static_assert(!kernel::validTaskCoreAffinity(1, 1));
	static_assert(!kernel::validTaskCoreAffinity(-2, 1));
	static_assert(kernel::validTaskCoreAffinity(1, 2));
	static_assert(kernel::validTaskPriority(7, 8));
	static_assert(!kernel::validTaskPriority(8, 8));

	std::uint32_t depth{};
	expect(kernel::taskStackDepthFromBytes(5, 4, 1, 100, depth) && depth == 2,
	    "rounding bytes to words");
	expect(!kernel::taskStackDepthFromBytes(0, 4, 1, 100, depth), "zero stack accepted");
	expect(!kernel::taskStackDepthFromBytes(UINT32_MAX, 4, 1, 100, depth), "oversized stack accepted");
	expect(!kernel::taskStackDepthFromBytes(1, 4, UINT32_MAX, UINT32_MAX, depth),
	    "overflowed stack bytes accepted");

	expect(!kernel::createTask(nullptr, nullptr, {}), "null entry accepted");
	expect(!kernel::createTask([](void*) {}, nullptr, {.priority = configMAX_PRIORITIES}),
	    "invalid priority accepted");
	expect(!kernel::createTask([](void*) {}, nullptr, {.coreAffinity = 1}),
	    "invalid affinity accepted");
	expect(!kernel::createTask([](void*) {}, nullptr, {.stackSizeBytes = 0}),
	    "empty stack accepted");

	expect(kernel::createTask([](void*) {}, nullptr, {.name = "test", .stackSizeBytes = 5}),
	    "create failed");
	expect(depths == 2 && kernel::taskCount() == 1 && suspended == 0,
	    "stack byte rounding or slot count");
	unsigned visits = 0;
	kernel::visitTasks([](kernel::TaskSnapshot const& snapshot, void* data) {
		expect(suspended == 0, "visitor runs under scheduler suspension");
		expect(snapshot.stackSizeBytes == 8 && snapshot.stackFreeBytes == 12,
		    "snapshot units changed");
		++*static_cast<unsigned*>(data);
		expect(kernel::taskCount() == 1, "visitor cannot inspect registry");
	},
	    &visits);
	expect(visits == 1, "snapshot omitted task");

	failCreate = true;
	expect(!kernel::createTask([](void*) {}, nullptr, {}) && kernel::taskCount() == 1,
	    "failed creation leaked slot");
	failCreate = false;
	preemptBeforeSuspend = true;
	runTask();
	expect(kernel::taskCount() == 1 && deletes == 0,
	    "racing task completion freed slot prematurely");
	expect(writes == 0, "disabled task startup logging emitted output");
	expect(kernel::reapFinishedTasks() == 1 && deletes == 1 && kernel::taskCount() == 1,
	    "maintenance did not reclaim suspended task");
	expect(kernel::reapFinishedTasks() == 0 && deletes == 1,
	    "maintenance reaped a task twice");
	expect(kernel::createTask([](void*) {}, nullptr, {}) && creates == 3,
	    "retired task slot not reusable");
	expect(deletes == 1 && kernel::taskCount() == 2,
	    "retired task reaping incorrect");
}
