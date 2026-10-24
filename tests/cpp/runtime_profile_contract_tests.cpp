#include <lexxos/app/program_input.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/lifecycle/event_bus.h>
#include <lexxos/program/program.h>
#include <lexxos/program/program_manager.h>
#include <lexxos/program/program_supervisor.h>
#include <lexxos/runtime/runtime_profile.h>
#include <lexxos/shell/shell.h>
#include <lexxos/vfs/vfs.h>

#include <cstdlib>
#include <iostream>

namespace {

void expect(bool condition, char const* message) {
	if (!condition) {
		std::cerr << "runtime profile contract test failed: " << message << '\n';
		std::exit(1);
	}
}

void checkProgramLimits(lexxos::runtime::RuntimeProfile const& profile) {
	expect(lexxos::program::ProgramManager::maxPrograms == profile.programs.maxPrograms,
	    "ProgramManager capacity follows runtime profile");
	expect(lexxos::program::ProgramSupervisor::maxProcesses == profile.programs.maxProcesses,
	    "ProgramSupervisor process capacity follows runtime profile");
	expect(lexxos::program::ProgramSupervisor::maxArguments == profile.programs.maxArguments,
	    "ProgramSupervisor argument capacity follows runtime profile");
	expect(lexxos::program::ProgramSupervisor::argumentStorageBytes == profile.programs.argumentStorageBytes,
	    "ProgramSupervisor argument storage follows runtime profile");
	expect(lexxos::program::ProgramSupervisor::minimumTaskStackBytes == profile.programs.minimumTaskStackBytes,
	    "ProgramSupervisor minimum task stack follows runtime profile");
	expect(lexxos::program::ProgramSupervisor::inlineTaskStackBytes == profile.programs.inlineTaskStackBytes,
	    "ProgramSupervisor inline task stack follows runtime profile");

	lexxos::program::ProgramRuntimeProfile const defaults{};
	expect(defaults.stackSizeBytes == profile.programs.defaultStackSizeBytes,
	    "default program stack follows runtime profile");
	expect(defaults.internalMemoryQuotaBytes == profile.programs.defaultInternalWorkspaceBytes,
	    "default internal workspace follows runtime profile");
	expect(defaults.externalMemoryQuotaBytes == profile.programs.defaultExternalWorkspaceBytes,
	    "default external workspace follows runtime profile");
}

void checkShellLimits(lexxos::runtime::RuntimeProfile const& profile) {
	expect(lexxos::shell::Shell::maxLineLength == profile.shell.maxLineLength,
	    "Shell line length follows runtime profile");
	expect(lexxos::shell::Shell::maxArguments == profile.shell.maxArguments,
	    "Shell argument capacity follows runtime profile");
	expect(lexxos::shell::Shell::maxHistoryEntries == profile.shell.maxHistoryEntries,
	    "Shell history capacity follows runtime profile");
}

void checkDeviceLimits(lexxos::runtime::RuntimeProfile const& profile) {
	expect(lexxos::devices::DeviceManager::maxI2cBuses == profile.devices.maxI2cBuses,
	    "DeviceManager I2C capacity follows runtime profile");
	expect(lexxos::devices::DeviceManager::maxBlockDevices == profile.devices.maxBlockDevices,
	    "DeviceManager block-device capacity follows runtime profile");
	expect(lexxos::devices::DeviceManager::maxDisplayDevices == profile.devices.maxDisplayDevices,
	    "DeviceManager display capacity follows runtime profile");
	expect(lexxos::devices::DeviceManager::maxInputDevices == profile.devices.maxInputDevices,
	    "DeviceManager input capacity follows runtime profile");
	expect(lexxos::devices::DeviceManager::maxPointingDevices == profile.devices.maxPointingDevices,
	    "DeviceManager pointing capacity follows runtime profile");
	expect(lexxos::devices::DeviceManager::maxGamepadDevices == profile.devices.maxGamepadDevices,
	    "DeviceManager gamepad capacity follows runtime profile");
	expect(lexxos::devices::DeviceManager::maxAudioDevices == profile.devices.maxAudioDevices,
	    "DeviceManager audio capacity follows runtime profile");
	expect(
	    lexxos::devices::DeviceManager::maxPicoCalcSouthbridgeServices == profile.devices.maxPicoCalcSouthbridgeServices,
	    "DeviceManager PicoCalc southbridge capacity follows runtime profile");
}

void checkVfsLimits(lexxos::runtime::RuntimeProfile const& profile) {
	expect(lexxos::vfs::maxPathLength == profile.vfs.maxPathLength,
	    "VFS path length follows runtime profile");
	expect(lexxos::vfs::maxNameLength == profile.vfs.maxNameLength,
	    "VFS name length follows runtime profile");
	expect(lexxos::vfs::detail::maxBlockDevices == profile.vfs.maxBlockDevices,
	    "VFS block-device capacity follows runtime profile");
	expect(lexxos::vfs::detail::maxFilesystemMounts == profile.vfs.maxFilesystemMounts,
	    "VFS mount capacity follows runtime profile");
	expect(lexxos::vfs::detail::maxTmpFiles == profile.vfs.maxTmpFiles,
	    "VFS tmpfs file capacity follows runtime profile");
	expect(lexxos::vfs::detail::maxTmpFileSize == profile.vfs.maxTmpFileSizeBytes,
	    "VFS tmpfs file size follows runtime profile");
	expect(lexxos::vfs::detail::maxFilesystemFactories == profile.vfs.maxFilesystemFactories,
	    "VFS factory capacity follows runtime profile");
}

void checkEventLimits(lexxos::runtime::RuntimeProfile const& profile) {
	expect(lexxos::lifecycle::EventBus::queueCapacity == profile.events.lifecycleQueueCapacity,
	    "lifecycle event queue follows runtime profile");
	expect(lexxos::app::program_input::queueCapacity == profile.events.programInputQueueCapacity,
	    "program input queue follows runtime profile");
}

} // namespace

void checkRuntimeProfile() {
	auto const& profile = lexxos::runtime::runtimeProfile();
	expect(&profile == &lexxos::runtime::standardRuntimeProfile, "runtimeProfile returns the standard profile");
	checkProgramLimits(profile);
	checkShellLimits(profile);
	checkDeviceLimits(profile);
	checkVfsLimits(profile);
	checkEventLimits(profile);
}
