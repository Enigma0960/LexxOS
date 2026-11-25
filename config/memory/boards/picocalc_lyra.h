#pragma once

#include <lexxos/runtime/runtime_profile.h>

// Product-level capacities. Values intentionally match the existing Lyra
// firmware; increase them only after examining .map and stack watermarks.
namespace lexxos::runtime::profiles {

constexpr void applyPicoCalcLyraBoard(RuntimeProfile& profile) {
	profile.origin.board = "picocalc_lyra";
	profile.shell.maxLineLength = 2048;
	profile.shell.maxArguments = 32;
	profile.shell.uartRxQueueBytes = 2048; // Holds a complete 1 KiB YMODEM packet while the task is descheduled.
	profile.shell.maxHistoryEntries = 10;
	profile.shell.injectedInputBytes = 4097; // ring buffer sacrifices one byte

	profile.programs.maxPrograms = 128;
	profile.programs.maxProcesses = 4;
	profile.programs.defaultStackSizeBytes = 2048;
	profile.programs.minimumTaskStackBytes = 4096;
	profile.programs.inlineTaskStackBytes = 4096;
	profile.programs.defaultInternalWorkspaceBytes = 0;
	profile.programs.defaultExternalWorkspaceBytes = 0;

	profile.events.lifecycleQueueCapacity = 16;
	profile.events.programInputQueueCapacity = 32;
	profile.ui.maxResolvedControls = 64;
	profile.ui.maxTreeNodes = 64;
	profile.graphics.maxImageHandles = 12;
	profile.graphics.maxImageDimension = 512;
	profile.graphics.maxImagePixels = 262144;
	profile.graphics.imageCacheBytes = 192 * 1024;
	profile.graphics.streamChunkBytes = 1024;
	// The remaining RuntimeProfile members inherit the base profile.
}

} // namespace lexxos::runtime::profiles
