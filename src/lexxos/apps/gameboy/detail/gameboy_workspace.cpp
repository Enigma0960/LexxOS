#include <lexxos/apps/gameboy/detail/gameboy_program_arguments.h>
#include <lexxos/apps/gameboy/detail/gameboy_program_support.h>

#include <new>

#if defined(LEXXOS_KERNEL_ESPIDF)
	#include <freertos/FreeRTOS.h>
	#include <freertos/task.h>
#elif defined(LEXXOS_KERNEL_FREERTOS)
	#include <FreeRTOS.h>
	#include <task.h>
#endif

namespace lexxos::apps::gameboy_programs {

GameBoyWorkspace* gameBoyWorkspaceStorage() {
	// Allocate optional emulator memory only when a Game Boy command is used.
	// ESP-IDF malloc can use configured PSRAM; allocation failure is reported below.
	static GameBoyWorkspace* workspace = nullptr;
	if (workspace != nullptr) {
		return workspace;
	}
#if defined(LEXXOS_KERNEL_FREERTOS)
	if (xPortGetFreeHeapSize() < sizeof(GameBoyWorkspace) + gameBoyWorkspaceHeapGuardBytes) {
		return nullptr;
	}

	void* memory = pvPortMalloc(sizeof(GameBoyWorkspace));
	if (memory == nullptr) {
		return nullptr;
	}
	workspace = new (memory) GameBoyWorkspace{};
#else
	workspace = new (std::nothrow) GameBoyWorkspace{};
#endif
	return workspace;
}

GameBoyWorkspace* gameBoyWorkspace(lexxos::os::AppContext& context) {
	auto* workspace = gameBoyWorkspaceStorage();
	if (workspace == nullptr) {
		context.writeErrorLine("gameboy: not enough memory for emulator workspace");
		context.write("workspace bytes: ");
		writeUnsigned64(context, sizeof(GameBoyWorkspace));
		context.write("\r\n");
#if defined(LEXXOS_KERNEL_FREERTOS) || defined(LEXXOS_KERNEL_ESPIDF)
		context.write("free heap bytes: ");
		writeUnsigned64(context, xPortGetFreeHeapSize());
		context.write("\r\n");
#endif
	}
	if (workspace != nullptr) {
		workspace->cartridge.bindPlatformServices(context.platform());
	}
	return workspace;
}

} // namespace lexxos::apps::gameboy_programs
