#include <cstddef>
#include <cstdint>

#include <lexxos/kernel/scheduler.h>
#include <lexxos/platform/rockchip/rk3506/cooperative_event.h>
#include <lexxos/runtime/selected_runtime_profile.h>

namespace {

// CPU0 only. Task 0 is the bootstrap/idle context on the original SVC stack.
// No task is preempted by the timer IRQ; the IRQ only advances the clock.
constexpr auto const& kMemory = lexxos::runtime::standardRuntimeProfile.kernel;
constexpr std::size_t kTaskSlots = kMemory.taskSlots;
constexpr std::size_t kStackPoolBytes = kMemory.taskStackPoolBytes;
static_assert(kTaskSlots >= 2);
static_assert(kStackPoolBytes >= 1024 && kStackPoolBytes % 16 == 0);

static_assert(kMemory.maximumTaskStackBytes <= kStackPoolBytes);
constexpr std::uint32_t kStackPattern = 0xA5A5A5A5U;
constexpr std::size_t kContextWords = 10; // r4-r11, r12, lr (task_switch.S)

struct Task {
	std::uint32_t* savedSp{};
	lexxos::kernel::TaskEntry entry{};
	void* userData{};
	lexxos::kernel::TaskSnapshot snapshot{};
	std::uint32_t wakeTick{};
	std::uint32_t* stackBase{};
	std::uint32_t stackWords{};
	bool allocated{};
	bool finished{};
	lexxos::platform::rk3506::CooperativeEvent* waitingEvent{};
};

alignas(16) std::uint32_t g_stackPool[kStackPoolBytes / sizeof(std::uint32_t)];
Task g_tasks[kTaskSlots]{};
std::size_t g_current{};
std::uint32_t g_totalSwitches{};
bool g_started{};
std::size_t g_handoffSlot{}; // 0 = no targeted handoff

extern "C" void rk3506_task_switch();
extern "C" std::uint32_t rk3506_timer_ticks();
extern "C" void rk3506_console_write(char const*);

bool reached(std::uint32_t now, std::uint32_t deadline) {
	return static_cast<std::int32_t>(now - deadline) >= 0;
}

bool runnable(Task const& task, std::uint32_t now) {
	return task.allocated && !task.finished && task.waitingEvent == nullptr && reached(now, task.wakeTick);
}

std::size_t alignUp(std::size_t n, std::size_t alignment) {
	return (n + alignment - 1) & ~(alignment - 1);
}

// Find a contiguous free range in the fixed stack pool. Completed tasks can
// give their ranges back only AFTER another task has taken over execution:
// the trampoline still runs on its own stack until rk3506_task_switch().
// CPU0-only cooperative scheduling means no other task can mutate this table
// while createTask() scans it; timer IRQs only update the tick counter.
std::uint32_t* findFreeStackRegion(std::size_t requestedWords) {
	constexpr std::size_t capacityWords = kStackPoolBytes / sizeof(std::uint32_t);
	std::size_t cursor = 0;
	while (cursor < capacityWords) {
		std::size_t nextBegin = capacityWords;
		std::size_t nextEnd = capacityWords;
		for (std::size_t i = 1; i < kTaskSlots; ++i) {
			Task const& task = g_tasks[i];
			if (!task.allocated || task.stackBase == nullptr || (task.finished && i != g_current)) {
				continue;
			}

			auto const begin = static_cast<std::size_t>(task.stackBase - g_stackPool);
			if (begin >= cursor && begin < nextBegin) {
				nextBegin = begin;
				nextEnd = begin + task.stackWords;
			}
		}

		if (requestedWords <= nextBegin - cursor) {
			return g_stackPool + cursor;
		}

		if (nextBegin == capacityWords) {
			break;
		}

		cursor = nextEnd;
	}

	return nullptr;
}

std::size_t remainingTasks() {
	std::size_t result = 0;
	for (std::size_t i = 1; i < kTaskSlots; ++i) {
		if (g_tasks[i].allocated && !g_tasks[i].finished) {
			++result;
		}
	}

	return result;
}

[[noreturn]] void taskTrampoline() {
	Task& current = g_tasks[g_current];
	current.snapshot.started = true;
	current.entry(current.userData);
	current.snapshot.started = false;
	current.finished = true;
	// Switch back to another runnable context; this stack must never resume.
	rk3506_task_switch();
	rk3506_console_write("FATAL: finished task resumed\n");
	for (;;) {
		asm volatile("wfi");
	}
}

std::uint32_t freeStackBytes(Task const& task) {
	if (!task.stackBase) {
		return 0;
	}

	std::uint32_t untouched = 0;
	// Conservative watermark: measure unmodified words from the bottom.
	while (untouched < task.stackWords && task.stackBase[untouched] == kStackPattern) {
		++untouched;
	}

	return untouched * sizeof(std::uint32_t);
}

} // namespace

extern "C" std::uint32_t* rk3506_scheduler_select_stack(std::uint32_t* previousSp) {
	g_tasks[g_current].savedSp = previousSp;
	auto const now = rk3506_timer_ticks();

	// A newly signalled driver gets exactly one immediate scheduling turn.
	// Ordinary scheduling remains round-robin; no global priority starvation.
	auto const handoff = g_handoffSlot;
	g_handoffSlot = 0;
	if (handoff > 0 && handoff < kTaskSlots && handoff != g_current && runnable(g_tasks[handoff], now)) {
		g_current = handoff;
		++g_totalSwitches;
		return g_tasks[handoff].savedSp;
	}

	for (std::size_t n = 1; n < kTaskSlots; ++n) {
		auto const next = (g_current + n) % kTaskSlots;
		Task const& candidate = g_tasks[next];
		if (next == 0 || runnable(candidate, now)) {
			g_current = next;
			++g_totalSwitches;
			return candidate.savedSp;
		}
	}

	return previousSp;
}

extern "C" std::uint32_t rk3506_scheduler_switches() {
	return g_totalSwitches;
}

namespace lexxos::kernel {

bool createTask(TaskEntry entry, void* userData, TaskOptions options) {
	if (!entry || (options.coreAffinity != anyTaskCore && options.coreAffinity != 0)) {
		return false;
	}

	if (options.stackSizeBytes < kMemory.minimumTaskStackBytes
	    || options.stackSizeBytes > kMemory.maximumTaskStackBytes) {
		return false;
	}

	// Programs start after the scheduler is running. A completed task slot can
	// be reused once its context has switched away from the old stack.
	std::size_t slot = 1;
	while (slot < kTaskSlots
	    && g_tasks[slot].allocated
	    && (!g_tasks[slot].finished || slot == g_current)) {
		++slot;
	}

	if (slot == kTaskSlots) {
		return false;
	}

	std::size_t const bytes = alignUp(options.stackSizeBytes, 16);
	std::size_t const words = bytes / sizeof(std::uint32_t);

	auto* bottom = findFreeStackRegion(words);
	if (bottom == nullptr) {
		return false;
	}

	for (std::size_t i = 0; i < words; ++i) {
		bottom[i] = kStackPattern;
	}

	std::uint32_t* frame = bottom + words - kContextWords;
	for (std::size_t i = 0; i < kContextWords; ++i) {
		frame[i] = 0;
	}

	frame[kContextWords - 1] = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&taskTrampoline));

	auto& task = g_tasks[slot];

	task = Task{}; // Forget any retired context before publishing the new task.
	task.savedSp = frame;
	task.entry = entry;
	task.userData = userData;
	task.snapshot = {
	    .name = options.name,
	    .stackSizeBytes = static_cast<std::uint32_t>(bytes),
	    .stackFreeBytes = static_cast<std::uint32_t>(bytes - kContextWords * sizeof(std::uint32_t)),
	    .priority = options.priority,
	    .started = false,
	    .executionClass = options.executionClass,
	    .coreAffinity = 0,
	};
	task.wakeTick = 0;
	task.stackBase = bottom;
	task.stackWords = static_cast<std::uint32_t>(words);
	task.allocated = true;
	return true;
}

void startScheduler() {
	if (g_started) {
		return;
	}

	g_tasks[0].allocated = true;
	g_current = 0;
	g_started = true;
	rk3506_console_write("CPU0 kernel scheduler: cooperative (TaskOptions API)\n");
	while (remainingTasks()) {
		rk3506_task_switch();
		// Idle until another IRQ if no task is runnable yet.
		bool ready = false;
		auto const now = rk3506_timer_ticks();
		for (std::size_t i = 1; i < kTaskSlots; ++i) {
			if (runnable(g_tasks[i], now)) {
				ready = true;
				break;
			}
		}

		if (!ready) {
			asm volatile("wfi" ::: "memory");
		}
	}
}

void sleepMs(std::uint32_t milliseconds) {
	if (!g_started || g_current == 0) {
		auto const until = rk3506_timer_ticks() + milliseconds;
		while (!reached(rk3506_timer_ticks(), until)) {
			asm volatile("wfi" ::: "memory");
		}

		return;
	}

	if (milliseconds > 0x7FFFFFFFU) {
		milliseconds = 0x7FFFFFFFU;
	}

	g_tasks[g_current].wakeTick = rk3506_timer_ticks() + milliseconds;
	rk3506_task_switch();
}

void yield() {
	if (g_started) {
		rk3506_task_switch();
	}
}

std::size_t taskCount() {
	return remainingTasks();
}

void visitTasks(TaskVisitor visitor, void* userData) {
	if (!visitor) {
		return;
	}

	for (std::size_t i = 1; i < kTaskSlots; ++i) {
		if (g_tasks[i].allocated && !g_tasks[i].finished) {
			auto snap = g_tasks[i].snapshot;
			snap.stackFreeBytes = freeStackBytes(g_tasks[i]);
			visitor(snap, userData);
		}
	}
}

extern "C" std::uint32_t rk3506_heap_free_bytes();
extern "C" std::uint32_t rk3506_heap_low_water_bytes();

MemorySnapshot memorySnapshot() {
	return {.available = true,
	    .freeHeapBytes = rk3506_heap_free_bytes(),
	    .minimumEverFreeHeapBytes = rk3506_heap_low_water_bytes()};
}

} // namespace lexxos::kernel

namespace lexxos::platform::rk3506 {

void waitCooperativeEvent(CooperativeEvent& event) {
	if (!g_started || g_current == 0) {
		return;
	}

	// No yield between checking the protected condition and parking:
	// producer and consumer run on the same cooperative CPU0.
	g_tasks[g_current].waitingEvent = &event;
	rk3506_task_switch();
}

void signalCooperativeEvent(CooperativeEvent& event) {
	// CPU0 task context only. The signal makes waiters runnable but does
	// not switch stacks: the caller can finish publishing shared state.
	auto const now = rk3506_timer_ticks();
	for (std::size_t i = 1; i < kTaskSlots; ++i) {
		Task& task = g_tasks[i];
		if (task.allocated && !task.finished && task.waitingEvent == &event) {
			task.waitingEvent = nullptr;
			task.wakeTick = now;
			if (g_handoffSlot == 0 || task.snapshot.priority > g_tasks[g_handoffSlot].snapshot.priority) {
				g_handoffSlot = i;
			}
		}
	}
}

} // namespace lexxos::platform::rk3506
