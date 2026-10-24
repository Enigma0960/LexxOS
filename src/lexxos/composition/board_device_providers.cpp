#include <lexxos/composition/board_device_providers.h>
#include <lexxos/composition/detail/platform_device_providers.h>

#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/feature.h>
#include <lexxos/platform/platform.h>
#include <lexxos/platform/platform_services.h>

#if FEATURE_ENABLED(SD_CARD)
	#include <lexxos/devices/sd_card.h>
#endif

namespace lexxos::composition {
using lexxos::devices::DeviceManager;
using lexxos::devices::I2cBus;
using lexxos::devices::debugI2cBus;
using lexxos::devices::systemI2cBus;

namespace detail {
	lexxos::platform::BoardDescriptor const& boardFor(lexxos::platform::PlatformServices const& platform) {
		auto const& board = platform.board;
		return board.current != nullptr ? board.current() : lexxos::platform::board();
	}

} // namespace detail

namespace {
	bool sameI2cPins(lexxos::platform::I2cPins const& left, lexxos::platform::I2cPins const& right) {
		return left.index == right.index && left.sda.number == right.sda.number && left.scl.number == right.scl.number && left.baudRate == right.baudRate;
	}

	I2cBus& initializeDebugI2cBus(DeviceManager& devices, lexxos::platform::BoardDescriptor const& activeBoard) {
		auto& debugBus = debugI2cBus();
		auto const debugPins = lexxos::platform::effectiveDebugI2c(activeBoard);
		if (debugPins.isValid() && !debugBus.isReady()) {
			debugBus.initialize(debugPins);
		}
		devices.registerI2cBus("debug", "debug/display I2C bus", debugBus);

		auto const systemPins = activeBoard.interfaces.systemI2c;
		if (systemPins.isValid()) {
			if (debugPins.isValid() && sameI2cPins(debugPins, systemPins)) {
				devices.registerI2cBus("system", "system board I2C bus", debugBus);
			} else {
				auto& systemBus = systemI2cBus();
				if (!systemBus.isReady()) {
					systemBus.initialize(systemPins);
				}
				devices.registerI2cBus("system", "system board I2C bus", systemBus);
			}
		}
		return debugBus;
	}

	void registerBlockDevices(DeviceManager& devices, lexxos::platform::PlatformServices const& platformServices) {
		(void) devices;
		(void) platformServices;
#if FEATURE_ENABLED(SD_CARD)
		if (auto* device = lexxos::devices::sd_card::blockDevice(platformServices); device != nullptr) {
			devices.registerBlockDevice("sd0", "SD card block device", *device);
		}
#endif
	}

} // namespace

void registerBoardDevices(DeviceManager& devices, lexxos::platform::PlatformServices const& platformServices) {
	auto const& activeBoard = detail::boardFor(platformServices);
	auto& debugBus = initializeDebugI2cBus(devices, activeBoard);
	registerBlockDevices(devices, platformServices);
#if defined(LEXXOS_PLATFORM_PICO)
	detail::registerPicoBoardDevices(devices, platformServices, activeBoard);
#elif defined(LEXXOS_PLATFORM_STM32F4)
	detail::registerStm32BoardDevices(devices, platformServices, activeBoard);
#elif defined(LEXXOS_PLATFORM_ESPIDF)
	detail::registerEspidfBoardDevices(devices, platformServices, activeBoard, debugBus);
#elif defined(LEXXOS_PLATFORM_SIM)
	detail::registerSimulatorDevices(devices);
#endif
	(void) debugBus;
}

} // namespace lexxos::composition
