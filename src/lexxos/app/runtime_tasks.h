#pragma once

#include <string_view>

#include <lexxos/feature.h>
#include <lexxos/program/program_manager.h>

namespace lexxos::shell {
class Shell;
}

namespace lexxos::os {
class OperatingSystem;
} // namespace lexxos::os

namespace lexxos::app {

#if FEATURE_ENABLED(DEBUG_DISPLAY)
class DebugStatusDisplay;
#endif

class SystemEventService;

struct RuntimeTaskContext {
	lexxos::os::OperatingSystem* operatingSystem{nullptr};
	lexxos::shell::Shell* shell{nullptr};
	SystemEventService* systemEvents{nullptr};

#if FEATURE_ENABLED(DEBUG_DISPLAY)
	DebugStatusDisplay* debugDisplay{nullptr};
	bool debugDisplayReady{false};
#endif
};

void registerShellProgram(lexxos::program::ProgramManager& programs);
[[nodiscard]] bool startRuntimeTasks(RuntimeTaskContext& context);

} // namespace lexxos::app
