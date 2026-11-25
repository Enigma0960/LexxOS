#include <lexxos/composition/products/lyra_runtime_providers.h>

#include <array>

#include <lexxos/app/rk3506/picosd_mount_policy.h>
#include <lexxos/app/rk3506/picosd_storage.h>
#include <lexxos/app/rk3506/picosd_storage_service.h>
#include <lexxos/apps/storage/rk3506/picosd_program.h>
#include <lexxos/apps/storage/rk3506/sd_hardware_bench_program.h>
#include <lexxos/apps/storage/rk3506/sdmmc_program.h>
#include <lexxos/composition/products/lyra_storage_service.h>
#include <lexxos/composition/products/picocalc_lyra_device_providers.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/os/operating_system.h>

namespace lexxos::platform::rk3506 {

void registerSdmmcFatProgram(program::ProgramManager& programs);
void registerDebugProgram(program::ProgramManager& programs);

} // namespace lexxos::platform::rk3506

namespace lexxos::composition::products {

namespace {

	bool configureStorage(void* context, std::span<hardware::products::StoragePolicy const> slots) {
		auto& os = *static_cast<lexxos::os::OperatingSystem*>(context);
		lyraSpi1Queue().setWaitFunction(os.platform().clock.sleepMs);

#if defined(LEXXOS_RK3506_SD_FORCE_READ_ONLY)
		// Recovery build: override only removable SPI1 sd1; never alter sd0.
		if (slots.size() != 2U) {
			return false;
		}

		std::array<hardware::products::StoragePolicy, 2> recoveryPolicy{
		    slots[0], slots[1]};
		recoveryPolicy[1].readOnlyByDefault = true;
		return lyraStorageService().configure(recoveryPolicy);
#else
		// Normal FreeRTOS and cooperative builds share the same board policy:
		// boot sd0 fixed RO, removable sd1 FAT RW.
		return lyraStorageService().configure(slots);
#endif
	}

	void systemStorage(void* context) {
		auto& os = *static_cast<lexxos::os::OperatingSystem*>(context);
		apps::storage_programs::registerSdmmcProgram(os.programs());
		platform::rk3506::registerSdmmcFatProgram(os.programs());
	}

	void debug(void* context) {
		auto& os = *static_cast<lexxos::os::OperatingSystem*>(context);
		platform::rk3506::registerDebugProgram(os.programs());
	}

	void removableStorage(void* context) {
		auto& os = *static_cast<lexxos::os::OperatingSystem*>(context);
		static app::rk3506::PicoSdMountPolicy mountPolicy;
		os.setStorageMountPolicy(&mountPolicy);
		platform::rk3506::registerPicoSdMountProgram(os.programs());
	}
#if defined(LEXXOS_RK3506_SD_DIAGNOSTICS)
	void removableDiagnostics(void* context) {
		auto& os = *static_cast<lexxos::os::OperatingSystem*>(context);
		os.programs().registerProgram(apps::storage_programs::SdHardwareBenchProgram::descriptor());
		platform::rk3506::registerPicoSdProgram(os.programs());
	}
#endif
	struct HotplugContext {
		app::rk3506::PicoSdStorageService& service;
		os::OperatingSystem& os;
	};

	void hotplugTask(void* context) {
		auto& task = *static_cast<HotplugContext*>(context);
		task.service.hotplug(task.os);
	}

	bool startDisplay(void*) {
		return startPicoCalcLyraDisplayTask();
	}

	bool startHotplug(void* context) {
		auto& os = *static_cast<lexxos::os::OperatingSystem*>(context);
		// The task outlives this call; both references have boot lifetime.
		static HotplugContext task{lyraStorageService(), os};
		if (&task.os != &os) {
			return false;
		}

		// Preserve the existing stack and scheduling policy during this migration.
		return kernel::createTask(hotplugTask, &task, {
		                                                  .name = "sd-hotplug",
		                                                  .stackSizeBytes = 12288,
		                                                  .priority = 1,
		                                                  .executionClass = kernel::TaskExecutionClass::Driver,
		                                              });
	}

} // namespace

LyraRuntimeProviders lyraRuntimeProviders(os::OperatingSystem& operatingSystem) {
	return {&operatingSystem, systemStorage, debug, removableStorage,
#if defined(LEXXOS_RK3506_SD_DIAGNOSTICS)
	    removableDiagnostics,
#else
	    nullptr,
#endif
	    startHotplug, configureStorage, startDisplay};
}

} // namespace lexxos::composition::products
