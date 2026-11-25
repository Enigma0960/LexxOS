#include <lexxos/kernel/scheduler.h>

namespace lexxos::kernel {

// Задачи с отдельными стеками предоставляет подключаемый планировщик.
// Для коротких обработчиков CE используется CooperativeLoop.
bool createTask(TaskEntry, void*, TaskOptions) {
	return false;
}

void startScheduler() {
}

std::size_t taskCount() {
	return 0;
}

void visitTasks(TaskVisitor, void*) {
}

std::size_t reapFinishedTasks() {
	return 0;
}

MemorySnapshot memorySnapshot() {
	return {};
}

void sleepMs(std::uint32_t milliseconds) {
	schedulerBackend().sleepMs(milliseconds);
}

void yield() {
	schedulerBackend().yield();
}

} // namespace lexxos::kernel
