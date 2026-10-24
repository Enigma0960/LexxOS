#include <lexxos/app/app.h>

extern "C" void app_main() {
	// ESP-IDF уже запустил FreeRTOS к моменту вызова app_main().
	// LexxOS создаёт собственные runtime-задачи и затем удаляет текущую app_main task.
	lexxos::app::run();
}
