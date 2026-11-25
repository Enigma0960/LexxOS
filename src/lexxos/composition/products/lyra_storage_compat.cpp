// Временные адаптеры команд и mount-policy без контекста.
#include <lexxos/app/rk3506/picosd_storage.h>
#include <lexxos/app/rk3506/picosd_storage_service.h>
#include <lexxos/composition/products/lyra_storage_service.h>
#include <lexxos/devices/rk3506/picosd_transport.h>
#include <lexxos/program/program_manager.h>

namespace lexxos::platform::rk3506 {

using lexxos::composition::products::lyraStorageService;

bool configurePicoSdStorage(std::span<lexxos::hardware::products::StoragePolicy const> slots) {
	return lyraStorageService().configure(slots);
}

lexxos::devices::SdCardBackend& picoSdBackend() {
	return lyraStorageService().backend();
}

void picoSdAutoSuppressUntilRemoval() {
	lyraStorageService().suppressUntilRemoval();
}

bool picoSdPrepareManualMount() {
	return lyraStorageService().prepareManualMount();
}

void picoSdManualMountFinished(bool success) {
	lyraStorageService().manualMountFinished(success);
}

void picoSdNotifyManualUnmount() {
	lyraStorageService().notifyManualUnmount();
}

void picoSdHotplugTask(void* context) {
	lyraStorageService().hotplug(*static_cast<lexxos::os::OperatingSystem*>(context));
}

bool picoSdFatMounted() {
	return lyraStorageService().fatMounted();
}

int picoSdRunMediaProgram(lexxos::os::AppContext& context, lexxos::program::ProgramInvocation const& invocation,
    lexxos::program::ParsedArguments const& arguments) {
	return lyraStorageService().run(context, invocation, arguments);
}

void registerPicoSdMountProgram(lexxos::program::ProgramManager& programs) {
	auto& service = lyraStorageService();
	if (!service.configured()) {
		return;
	}

	picoSdSetMediaHooks(lexxos::composition::products::lyraStorageMediaHooks(service));
	programs.registerProgram({
	    .name = "sd-media",
	    .help = "PicoCalc removable SD FAT mount and hotplug status",
	    .description = "Board-configured sd1 media with standard FatFs and VFS",
	    .usage = "sd-media [probe | mount | status | unmount | recover | auto on|off | rw on|off]",
	    .handler = lexxos::platform::rk3506::picoSdRunMediaProgram,
	    .runtime = {
	                .launchMode = lexxos::program::ProgramLaunchMode::ForegroundTask,
	                .stackSizeBytes = 16384,
	                .priority = 1,
	                },
	    .allowExtraArguments = true,
	    .enabled = true,
	});
}

} // namespace lexxos::platform::rk3506
