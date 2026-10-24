#include <FreeRTOS.h>
#include <task.h>

#if defined(LEXXOS_PLATFORM_PICO)
	#include <pico/stdlib.h>
#endif

#include <lexxos/kernel/scheduler.h>

namespace {
[[noreturn]] void halt() {
	taskDISABLE_INTERRUPTS();
	while (true) {
#if defined(LEXXOS_PLATFORM_PICO)
		tight_loop_contents();
#else
		__asm volatile("wfi");
#endif
	}
}

} // namespace

extern "C" void vApplicationMallocFailedHook(void) {
	lexxos::kernel::schedulerBackend().writeConsole("panic: FreeRTOS malloc failed\r\n");
	halt();
}

extern "C" void vApplicationStackOverflowHook(TaskHandle_t, char* taskName) {
	lexxos::kernel::schedulerBackend().writeConsole("panic: FreeRTOS stack overflow in task ");
	if (taskName != nullptr) {
		lexxos::kernel::schedulerBackend().writeConsole(taskName);
	} else {
		lexxos::kernel::schedulerBackend().writeConsole("<unknown>");
	}
	lexxos::kernel::schedulerBackend().writeConsole("\r\n");
	halt();
}
