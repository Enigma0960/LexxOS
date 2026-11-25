#pragma once

#include <cstddef>
#include <cstdint>

namespace lexxos::runtime {

struct ProgramRuntimeLimits {
	std::size_t maxPrograms{128};
	std::size_t maxProcesses{4};
	std::size_t maxArguments{32};
	std::size_t argumentStorageBytes{2048};
	std::uint32_t defaultStackSizeBytes{2048};
	std::uint32_t minimumTaskStackBytes{4096};
	std::uint32_t inlineTaskStackBytes{4096};
	std::uint32_t filesystemMutationStackBytes{4096};
	std::uint32_t networkStatusStackBytes{4096};
	std::uint32_t foregroundCleanupDelayMs{50};
	std::uint32_t defaultInternalWorkspaceBytes{0};
	std::uint32_t defaultExternalWorkspaceBytes{0};
};

struct ShellRuntimeLimits {
	std::size_t maxLineLength{2048};
	// Software RX queue for polled UART consoles; excludes the UART FIFO.
	std::size_t uartRxQueueBytes{64};
	std::size_t maxArguments{32};
	std::size_t maxHistoryEntries{10};
	// Ring buffer keeps one byte empty, so 4097 stores one full 4 KiB paste.
	std::size_t injectedInputBytes{4097};
};

struct DeviceRuntimeLimits {
	std::size_t maxI2cBuses{4};
	std::size_t maxSpiBuses{4};
	std::size_t maxUartBuses{4};
	std::size_t maxGpioPins{16};
	std::size_t maxBlockDevices{4};
	std::size_t maxDisplayDevices{4};
	std::size_t maxInputDevices{4};
	std::size_t maxPointingDevices{2};
	std::size_t maxGamepadDevices{4};
	std::size_t maxAudioDevices{1};
	std::size_t maxMicrophoneDevices{1};
	std::size_t maxSensorDevices{4};
	std::size_t maxInfraredDevices{2};
	std::size_t maxPicoCalcSouthbridgeServices{2};
};

struct VfsRuntimeLimits {
	std::size_t maxPathLength{128};
	std::size_t maxNameLength{64};
	std::size_t maxBlockDevices{4};
	std::size_t maxFilesystemMounts{4};
	std::size_t maxTmpFiles{8};
	std::size_t maxTmpFileSizeBytes{1024};
	std::size_t maxFilesystemFactories{3};
};

struct EventRuntimeLimits {
	std::uint32_t lifecycleQueueCapacity{16};
	std::uint32_t programInputQueueCapacity{32};
};

// Host-side resolved control storage; the v2 wire protocol still allows
// at most 16 nodes including non-rendering layout containers.
struct GraphicsRuntimeLimits {
	std::size_t maxImageHandles{8};
	std::uint32_t maxImageDimension{512};
	std::uint32_t maxImagePixels{131072};
	std::size_t imageCacheBytes{192 * 1024};
	std::size_t streamChunkBytes{1024};
};

struct UiRuntimeLimits {
	std::size_t maxResolvedControls{32};
	std::size_t maxTreeNodes{32};
	// Bounded per-view item tables; each table keeps one 8-entry wire page.
	std::size_t maxListStores{8};
};

struct RuntimeTaskStackProfile {
	std::uint32_t consoleTaskBytes{12288};
	std::uint32_t systemEventTaskBytes{8192};
	std::uint32_t consoleMirrorTaskBytes{4096};
	std::uint32_t localConsoleInputTaskBytes{4096};
	std::uint32_t healthTaskBytes{1024};
	std::uint32_t debugDisplayTaskBytes{1024};
	std::uint32_t displayPowerTaskBytes{3072};
};

// Zero indicates that the platform does not use this particular allocator.
// For RK3506 these are compile-time capacities, not runtime heap quotas.
struct KernelRuntimeLimits {
	std::size_t heapBytes{0};
	std::size_t taskStackPoolBytes{0};
	std::size_t taskSlots{0};
	std::uint32_t minimumTaskStackBytes{0};
	std::uint32_t maximumTaskStackBytes{0};
};

struct MemoryProfileOrigin {
	char const* chip{"base"};
	char const* platform{"base"};
	char const* board{"base"};
};

struct RuntimeProfile {
	KernelRuntimeLimits kernel{};
	ProgramRuntimeLimits programs{};
	ShellRuntimeLimits shell{};
	DeviceRuntimeLimits devices{};
	VfsRuntimeLimits vfs{};
	EventRuntimeLimits events{};
	UiRuntimeLimits ui{};
	GraphicsRuntimeLimits graphics{};
	RuntimeTaskStackProfile taskStacks{};
	MemoryProfileOrigin origin{};
};

[[nodiscard]] constexpr bool validateRuntimeProfile(RuntimeProfile const& profile) {
	if (profile.shell.maxLineLength == 0
	    || profile.shell.uartRxQueueBytes == 0
	    || profile.shell.uartRxQueueBytes > profile.shell.maxLineLength
	    || profile.shell.injectedInputBytes <= profile.shell.maxLineLength
	    || profile.shell.maxHistoryEntries == 0
	    || profile.programs.maxProcesses == 0
	    || profile.programs.maxPrograms == 0
	    || profile.events.lifecycleQueueCapacity < 2
	    || profile.events.programInputQueueCapacity < 2
	    || profile.ui.maxResolvedControls < 8
	    || profile.ui.maxResolvedControls > profile.ui.maxTreeNodes
	    || profile.ui.maxTreeNodes < 16
	    || profile.ui.maxTreeNodes > 256
	    || profile.ui.maxListStores == 0
	    || profile.ui.maxListStores > profile.ui.maxResolvedControls
	    || profile.graphics.maxImageHandles == 0
	    || profile.graphics.maxImageHandles > 64
	    || profile.graphics.maxImageDimension == 0
	    || profile.graphics.maxImageDimension > 1024
	    || profile.graphics.maxImagePixels == 0
	    || profile.graphics.maxImagePixels > 1048576
	    || profile.graphics.imageCacheBytes < 1024
	    || profile.graphics.imageCacheBytes > 16U * 1024U * 1024U
	    || profile.graphics.streamChunkBytes < 256
	    || profile.graphics.streamChunkBytes > 4096) {
		return false;
	}

	if (profile.kernel.taskSlots == 0) {
		// SDK-managed schedulers have no LexxOS task-slot or stack-pool array.
		// Pico FreeRTOS may still expose its configured heap_4 capacity.
		return profile.kernel.taskStackPoolBytes == 0
		    && profile.kernel.minimumTaskStackBytes == 0
		    && profile.kernel.maximumTaskStackBytes == 0
		    && (profile.kernel.heapBytes == 0 || profile.kernel.heapBytes >= 1024);
	}

	return profile.kernel.taskSlots >= 2
	    && profile.kernel.heapBytes >= 1024
	    && profile.kernel.taskStackPoolBytes >= 1024
	    && profile.kernel.minimumTaskStackBytes >= 512
	    && profile.kernel.maximumTaskStackBytes >= profile.kernel.minimumTaskStackBytes
	    && profile.kernel.maximumTaskStackBytes <= profile.kernel.taskStackPoolBytes
	    && profile.programs.minimumTaskStackBytes <= profile.kernel.maximumTaskStackBytes
	    && profile.programs.inlineTaskStackBytes <= profile.kernel.maximumTaskStackBytes
	    && profile.taskStacks.consoleTaskBytes <= profile.kernel.maximumTaskStackBytes
	    && profile.taskStacks.systemEventTaskBytes <= profile.kernel.maximumTaskStackBytes
	    && profile.taskStacks.consoleMirrorTaskBytes <= profile.kernel.maximumTaskStackBytes
	    && profile.taskStacks.localConsoleInputTaskBytes <= profile.kernel.maximumTaskStackBytes;
}

} // namespace lexxos::runtime
