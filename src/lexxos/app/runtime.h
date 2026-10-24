#pragma once

#include <lexxos/app/runtime_tasks.h>
#include <lexxos/app/system_event_service.h>
#include <lexxos/feature.h>
#include <lexxos/os/operating_system.h>
#include <lexxos/platform/platform_services.h>
#include <lexxos/shell/shell.h>

#if FEATURE_ENABLED(DEBUG_DISPLAY)
	#include <lexxos/app/debug_status_display.h>
#endif

#if FEATURE_ENABLED(SD_CARD)
	#include <lexxos/app/sd_automount_service.h>
#endif

namespace lexxos::app {

class Runtime {
public:
	int run();

private:
	void initializeSystem();

#if FEATURE_ENABLED(DEBUG_DISPLAY)
	bool initializeDebugDisplay();
#endif

#if FEATURE_ENABLED(SD_CARD)
	void initializeStorage();
#endif

	lexxos::platform::PlatformServices const& _platformServices{lexxos::platform::services()};
	lexxos::shell::Shell _shell;
	lexxos::os::OperatingSystem _operatingSystem{_platformServices};
	RuntimeTaskContext _taskContext{};
	SystemEventService _systemEvents{_platformServices};

#if FEATURE_ENABLED(SD_CARD)
	SdAutomountService _sdAutomount{_operatingSystem};
#endif

#if FEATURE_ENABLED(DEBUG_DISPLAY)
	DebugStatusDisplay _debugDisplay{_operatingSystem};
#endif
};

} // namespace lexxos::app
