#include <lexxos/app/scoped_console_mirror_suspension.h>

#include <lexxos/app/console_mirror.h>

#if defined(LEXXOS_KERNEL_FREERTOS)
	#include <FreeRTOS.h>
	#include <task.h>
#endif

namespace lexxos::app {

namespace {

	unsigned suspensionCount{};

	struct SuspensionGuard {
		SuspensionGuard() {
#if defined(LEXXOS_KERNEL_FREERTOS)
			taskENTER_CRITICAL();
#endif
		}

		~SuspensionGuard() {
#if defined(LEXXOS_KERNEL_FREERTOS)
			taskEXIT_CRITICAL();
#endif
		}
	};

} // namespace

ScopedConsoleMirrorSuspension::ScopedConsoleMirrorSuspension() {
	SuspensionGuard guard;
	++suspensionCount;
}

ScopedConsoleMirrorSuspension::~ScopedConsoleMirrorSuspension() {
	SuspensionGuard guard;
	--suspensionCount;
}

namespace console_mirror {

	bool isSuspended() {
		SuspensionGuard guard;
		return suspensionCount != 0;
	}

} // namespace lexxos::app::console_mirror

} // namespace lexxos::app
