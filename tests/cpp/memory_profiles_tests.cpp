#include <lexxos/runtime/selected_runtime_profile.h>

using lexxos::runtime::RuntimeProfile;
using lexxos::runtime::standardRuntimeProfile;

// Freeze the known working bring-up numbers: this patch is a refactoring.
static_assert(standardRuntimeProfile.kernel.heapBytes == 256 * 1024);
static_assert(standardRuntimeProfile.kernel.taskStackPoolBytes == 96 * 1024);
static_assert(standardRuntimeProfile.kernel.taskSlots == 8);
static_assert(standardRuntimeProfile.kernel.maximumTaskStackBytes == 32768);
static_assert(standardRuntimeProfile.taskStacks.consoleTaskBytes == 12288);
static_assert(standardRuntimeProfile.taskStacks.consoleMirrorTaskBytes == 8192);
static_assert(standardRuntimeProfile.shell.maxLineLength == 2048);
static_assert(standardRuntimeProfile.shell.injectedInputBytes == 4097);
static_assert(standardRuntimeProfile.shell.maxHistoryEntries == 10);
static_assert(standardRuntimeProfile.programs.maxProcesses == 4);
static_assert(standardRuntimeProfile.programs.defaultInternalWorkspaceBytes == 0);
static_assert(standardRuntimeProfile.events.lifecycleQueueCapacity == 16);
static_assert(standardRuntimeProfile.ui.maxResolvedControls == 64);
static_assert(standardRuntimeProfile.ui.maxTreeNodes == 64);
static_assert(lexxos::runtime::validateRuntimeProfile(standardRuntimeProfile));

// Verify precedence without adding a second RuntimeProfile type.
constexpr RuntimeProfile overrideOrder = [] {
	RuntimeProfile profile{};
	profile.shell.maxHistoryEntries = 3;
	lexxos::runtime::profiles::applyRk3506g2Chip(profile);
	lexxos::runtime::profiles::applyRk3506Platform(profile);
	lexxos::runtime::profiles::applyPicoCalcLyraBoard(profile);
	return profile;
}();
static_assert(overrideOrder.shell.maxHistoryEntries == 10);
static_assert(overrideOrder.ui.maxResolvedControls == 64);
static_assert(overrideOrder.ui.maxTreeNodes == 64);

constexpr RuntimeProfile invalidPasteBuffer = [] {
	RuntimeProfile profile = standardRuntimeProfile;
	profile.shell.injectedInputBytes = profile.shell.maxLineLength;
	return profile;
}();
static_assert(!lexxos::runtime::validateRuntimeProfile(invalidPasteBuffer));

constexpr RuntimeProfile invalidUiLimit = [] {
	RuntimeProfile profile = standardRuntimeProfile;
	profile.ui.maxResolvedControls = 257;
	profile.ui.maxTreeNodes = 256;
	return profile;
}();
static_assert(!lexxos::runtime::validateRuntimeProfile(invalidUiLimit));

int main() {
	return 0;
}
