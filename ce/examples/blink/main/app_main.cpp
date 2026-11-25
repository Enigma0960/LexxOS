#include "board.h"

#include <lexxos/core/core_system.h>
#include <lexxos/kernel/cooperative/cooperative_loop.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/platform/platform_services.h>

#include "blink_application.h"

#if defined(LEXXOS_CE_EXECUTION_FREERTOS)
namespace {

void blinkTask(void* argument) {
	auto& application = *static_cast<lexxos::ce::blink::BlinkApplication*>(argument);

	while (application.step()) {
	}

	// При ошибке GPIO задача остаётся в безопасном ожидании, не повторяя запись и не занимая CPU.
	for (;;) {
		lexxos::kernel::sleepMs(1000);
	}
}

} // namespace
#endif

extern "C" void app_main() {
	static lexxos::core::CoreSystem system{lexxos::ce::blink::platformServices()};
	static lexxos::ce::blink::BlinkApplication application{system};

	if (!application.initialize()) {
		return;
	}

#if defined(LEXXOS_CE_EXECUTION_FREERTOS)
	if (!lexxos::kernel::createTask(blinkTask, &application,
	        {.name = "ce-led", .stackSizeBytes = 4096, .priority = 1})) {
		system.platform().console.write("[error] The LED task has not been created; the LED remains off.\r\n");
		return;
	}

	system.platform().console.write("[core] The CE-led task has been launched.\r\n");
	// Возврат освобождает app_main task; FreeRTOS уже работает.
#else
	auto const& clock = system.platform().clock;
	if (!clock.uptimeMs) {
		return;
	}

	lexxos::kernel::CooperativeLoop loop;
	if (!loop.add([](void* context) {
		    return static_cast<lexxos::ce::blink::BlinkApplication*>(context)->tick();
	    },
	        &application, 1000, clock.uptimeMs())) {
		return;
	}
	system.platform().console.write("[core] cooperative LED loop started.\r\n");
	while (loop.taskCount()) {
		loop.poll(clock.uptimeMs());
		clock.sleepMs(1);
	}
#endif
}
