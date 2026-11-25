#include <lexxos/composition/detail/platform_device_providers.h>
#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/feature.h>
#include <lexxos/platform/platform_services.h>
#if FEATURE_ENABLED(SD_CARD)
	#include <lexxos/devices/sd_card.h>
#endif

namespace lexxos::composition::detail {

using devices::debugI2cBus;
using devices::DeviceManager;
using devices::I2cBus;
using devices::systemI2cBus;

namespace {

	bool sameI2cPins(lexxos::platform::I2cPins const& left, lexxos::platform::I2cPins const& right) {
		return left.index == right.index && left.sda.number == right.sda.number && left.scl.number == right.scl.number && left.baudRate == right.baudRate;
	}

	I2cBus& initializeDebugI2cBus(DeviceManager& devices, lexxos::platform::LegacyBoardDescriptor const& activeBoard) {
		auto& debugBus = debugI2cBus();
		auto const debugPins = lexxos::platform::effectiveDebugI2c(activeBoard);
		if (debugPins.isValid() && !debugBus.isReady()) {
			debugBus.initialize(debugPins);
		}

		devices.registerI2cBus("debug", "tools/debug/display I2C bus", debugBus);

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

} // namespace

void registerLegacyBoardDevices(DeviceManager& devices, platform::PlatformServices const& platformServices,
    platform::LegacyBoardDescriptor const& activeBoard) {
	for (std::uint8_t index = 0;
	    index < activeBoard.userGpio.count
	    && index < lexxos::platform::maxBoardUserGpioPins;
	    ++index) {
		auto const& pin = activeBoard.userGpio.pins[index];
		if (!pin.isValid() || pin.driver != lexxos::platform::GpioDriver::None) {
			continue;
		}

		(void) devices.registerGpioPin(
		    pin.label, "board user GPIO", pin.pin,
		    pin.inputAllowed, pin.outputAllowed);
	}

	initializeDebugI2cBus(devices, activeBoard);
	(void) platformServices;
#if FEATURE_ENABLED(SD_CARD)
	if (auto* device = lexxos::devices::sd_card::blockDevice(platformServices); device != nullptr) {
		devices.registerBlockDevice("sd0", "SD card block device", *device);
	}
#endif
}

} // namespace lexxos::composition::detail
