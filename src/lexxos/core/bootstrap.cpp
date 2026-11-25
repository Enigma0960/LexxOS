#include <lexxos/core/bootstrap.h>

#include <lexxos/core/core_system.h>
#include <lexxos/kernel/scheduler.h>
#include <lexxos/kernel/system.h>
#include <lexxos/platform/platform_services.h>

namespace lexxos::core {

void initializePlatform(lexxos::platform::PlatformServices const& platform) {
	lexxos::kernel::bindSchedulerBackend(platform);
	if (platform.lifecycle.initialize != nullptr) {
		platform.lifecycle.initialize();
	}

	lexxos::kernel::resetShutdownRequest();
}

bool bootstrap(CoreSystem& system, DeviceRegistrar registerDevices) {
	initializePlatform(system.platform());
	system.initialize();
	auto const& platform = system.platform();
	if (platform.board.current) {
		auto const& board = platform.board.current();
		auto& devices = system.devices();
		if (!devices.bindBoardAnalogPwm(board, platform) || !devices.bindBoardSpi(board, platform) || !devices.bindBoardI2c(board, platform) || !devices.bindBoardGpio(board.userGpio, platform.gpio)) {
			devices.initialize();
			return false;
		}
	}

	if (registerDevices != nullptr) {
		registerDevices(system.devices(), system.platform());
	}

	system.refreshCapabilities();
	return true;
}

} // namespace lexxos::core
