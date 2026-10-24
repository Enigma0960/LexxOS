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
	std::uint32_t foregroundCleanupDelayMs{50};
	std::uint32_t defaultInternalWorkspaceBytes{0};
	std::uint32_t defaultExternalWorkspaceBytes{0};
};

struct ShellRuntimeLimits {
	std::size_t maxLineLength{2048};
	std::size_t maxArguments{32};
	std::size_t maxHistoryEntries{10};
	// Ring buffer keeps one byte empty, so 4097 stores one full 4 KiB paste.
	std::size_t injectedInputBytes{4097};
};

struct DeviceRuntimeLimits {
	std::size_t maxI2cBuses{4};
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

struct RuntimeTaskStackProfile {
	std::uint32_t consoleTaskBytes{12288};
	std::uint32_t systemEventTaskBytes{8192};
	std::uint32_t consoleMirrorTaskBytes{4096};
	std::uint32_t localConsoleInputTaskBytes{4096};
	std::uint32_t healthTaskBytes{1024};
	std::uint32_t debugDisplayTaskBytes{1024};
};

struct RuntimeProfile {
	ProgramRuntimeLimits programs{};
	ShellRuntimeLimits shell{};
	DeviceRuntimeLimits devices{};
	VfsRuntimeLimits vfs{};
	EventRuntimeLimits events{};
	RuntimeTaskStackProfile taskStacks{};
};

inline constexpr RuntimeProfile standardRuntimeProfile{};

[[nodiscard]] constexpr RuntimeProfile const& runtimeProfile() {
	return standardRuntimeProfile;
}

} // namespace lexxos::runtime
