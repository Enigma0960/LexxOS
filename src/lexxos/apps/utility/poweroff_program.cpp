#include <lexxos/apps/utility/poweroff_program.h>

#include <array>
#include <cstring>
#include <string_view>

#include <lexxos/app/picocalc_display_power.h>
#include <lexxos/apps/common/core_programs_support.h>
#include <lexxos/apps/utility/detail/poweroff_mount_targets.h>
#include <lexxos/devices/display_device.h>
#include <lexxos/feature.h>
#include <lexxos/kernel/system.h>
#include <lexxos/lifecycle/event_bus.h>
#include <lexxos/os/app_context.h>
#include <lexxos/vfs/vfs.h>

#if FEATURE_ENABLED(PICO_CALC)
	#include <lexxos/devices/picocalc/picocalc_southbridge.h>
	#include <lexxos/devices/picocalc/picocalc_southbridge_resource_policy.h>
	#include <lexxos/devices/picocalc/picocalc_southbridge_runtime.h>
#endif

namespace lexxos::apps::utility_programs {
namespace {

	using detail::MountTargets;

	bool copyString(std::string_view value, std::array<char, lexxos::vfs::maxPathLength>& output, std::size_t& length) {
		if (value.empty() || value.size() >= output.size()) {
			return false;
		}
		std::memcpy(output.data(), value.data(), value.size());
		output[value.size()] = '\0';
		length = value.size();
		return true;
	}

	bool collectFilesystemMount(lexxos::vfs::MountInfo const& mount, void* userData) {
		auto& targets = *static_cast<MountTargets*>(userData);
		if (mount.type == "virtualfs" || mount.type == "block") {
			return true;
		}
		if (targets.count >= targets.targets.size()) {
			targets.overflow = true;
			return false;
		}
		if (!copyString(mount.target, targets.targets[targets.count], targets.lengths[targets.count])) {
			targets.overflow = true;
			return false;
		}
		++targets.count;
		return true;
	}

	bool unmountFilesystems(lexxos::os::AppContext& context) {
		MountTargets targets{};
		(void) lexxos::vfs::visitMounts(collectFilesystemMount, &targets);
		if (targets.overflow) {
			context.writeErrorLine("poweroff: too many mounted filesystems to unmount safely");
			return false;
		}

		for (std::size_t index = 0; index < targets.count; ++index) {
			std::string_view const target{targets.targets[index].data(), targets.lengths[index]};
			auto const status = lexxos::vfs::unmountFilesystem(target);
			if (status != lexxos::vfs::UnmountStatus::Ok) {
				context.writeError("poweroff: unmount failed for ");
				context.writeError(target);
				context.writeError(": ");
				context.writeErrorLine(lexxos::vfs::toString(status));
				return false;
			}
			context.write("poweroff: unmounted ");
			context.writeLine(target);
		}
		return true;
	}

#if FEATURE_ENABLED(PICO_CALC)
	lexxos::devices::PicoCalcSouthbridgeRuntimeService* picoCalcSouthbridge(lexxos::os::AppContext& context) {
		return lexxos::devices::PicoCalcSouthbridgeResourcePolicy::readyService(context.devices());
	}

	lexxos::devices::DeviceResourceLock lockPicoCalcSouthbridge(lexxos::os::AppContext& context) {
		return lexxos::devices::PicoCalcSouthbridgeResourcePolicy::lock(
		    context.devices(),
		    "poweroff",
		    lexxos::devices::DeviceResourcePriority::Critical);
	}
#endif

} // namespace

lexxos::program::ProgramDescriptor PoweroffProgram::descriptor() {
	return {
	    .name = "poweroff",
	    .help = "power off target device",
	    .description = "Synchronizes mounted filesystems and requests board power-off when supported.",
	    .usage = "poweroff",
	    .handler = run,
	    .legacy = false,
	    .enabled = true,
	};
}

int PoweroffProgram::run(
    lexxos::os::AppContext& context,
    lexxos::program::ProgramInvocation const&,
    lexxos::program::ParsedArguments const&) {
	(void) lexxos::lifecycle::defaultEventBus().publish({
	    .type = lexxos::lifecycle::EventType::PowerOffRequested,
	    .timestampMs = lexxos::apps::uptimeMs(context),
	});
#if FEATURE_ENABLED(PICO_CALC)
	auto* service = picoCalcSouthbridge(context);
	if (service == nullptr) {
		context.writeErrorLine("poweroff: board power service unavailable");
		return 1;
	}

	auto serviceLock = lockPicoCalcSouthbridge(context);
	if (!serviceLock) {
		context.writeErrorLine("poweroff: southbridge service is busy");
		return 1;
	}

	bool const refreshed = service->refreshSouthbridgeSnapshot(lexxos::apps::uptimeMs(context));
	(void) refreshed;
	lexxos::devices::PicoCalcSouthbridgeSnapshot snapshot{};
	if (service->southbridgeSnapshot(snapshot)
	    && snapshot.has(lexxos::devices::PicoCalcSouthbridgeSnapshot::validPowerOffSupport)
	    && snapshot.powerOffSupport == 0) {
		context.writeErrorLine("poweroff: power-off is not supported by southbridge");
		return 1;
	}
	serviceLock.release();

	auto const syncStatus = lexxos::vfs::sync();
	if (syncStatus != lexxos::vfs::SyncStatus::Ok) {
		context.writeError("poweroff: sync failed: ");
		context.writeErrorLine(lexxos::vfs::toString(syncStatus));
		return 1;
	}
	context.writeLine("poweroff: sync ok");

	if (!unmountFilesystems(context)) {
		return 1;
	}

	serviceLock = lockPicoCalcSouthbridge(context);
	if (!serviceLock) {
		context.writeErrorLine("poweroff: southbridge service is busy");
		return 1;
	}

	if (!service->requestPowerOff(lexxos::apps::uptimeMs(context))) {
		context.writeErrorLine("poweroff: southbridge request failed");
		return 1;
	}

	context.writeLine("poweroff: requested");
	(void) lexxos::lifecycle::defaultEventBus().publish({
	    .type = lexxos::lifecycle::EventType::ShutdownRequested,
	    .timestampMs = lexxos::apps::uptimeMs(context),
	});
	lexxos::kernel::requestShutdown();
	return 0;
#else
	auto const syncStatus = lexxos::vfs::sync();
	if (syncStatus != lexxos::vfs::SyncStatus::Ok) {
		context.writeError("poweroff: sync failed: ");
		context.writeErrorLine(lexxos::vfs::toString(syncStatus));
		return 1;
	}
	context.writeLine("poweroff: sync ok");

	if (!unmountFilesystems(context)) {
		return 1;
	}

	#if FEATURE_ENABLED(DISPLAY)
	if (auto* display = context.devices().primaryDisplayDevice(); display != nullptr && display->supportsPowerMode()) {
		lexxos::app::picocalc_display_power::ApplyResult result{};
		(void) lexxos::app::picocalc_display_power::applyPowerMode(
		    context.devices(),
		    context.platform(),
		    *display,
		    lexxos::devices::DisplayPowerMode::Off,
		    "poweroff",
		    lexxos::devices::DeviceResourcePriority::Critical,
		    &result);
		if (!result.displayLockOk) {
			context.writeErrorLine("poweroff: display device is busy");
			return 1;
		}
		if (!result.displayOk) {
			context.writeErrorLine("poweroff: display power-off failed");
			return 1;
		}
		context.writeLine("poweroff: display off");
	}
	#endif

	(void) lexxos::lifecycle::defaultEventBus().publish({
	    .type = lexxos::lifecycle::EventType::ShutdownRequested,
	    .timestampMs = lexxos::apps::uptimeMs(context),
	});
	context.writeLine("poweroff: halted; board power remains supplied");
	lexxos::kernel::requestShutdown();
	return 0;
#endif
}

} // namespace lexxos::apps::utility_programs
