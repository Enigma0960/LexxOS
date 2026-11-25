#include <lexxos/composition/board_device_providers.h>

#include <lexxos/composition/detail/platform_device_providers.h>
#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/bus/spi_bus.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/feature.h>
#include <lexxos/platform/platform.h>
#include <lexxos/platform/platform_services.h>

namespace lexxos::composition {

using lexxos::devices::DeviceManager;

namespace detail {

	lexxos::platform::LegacyBoardDescriptor const& boardFor(lexxos::platform::PlatformServices const& platform) {
		auto const& board = platform.board;
		return board.current != nullptr ? board.current() : lexxos::platform::board();
	}

} // namespace lexxos::composition::detail

bool registerBoardDevices(DeviceManager& devices, platform::PlatformServices const& platformServices) {
	return registerBoardDevices(devices, platformServices, detail::selectedBoardDeviceProviders());
}

bool registerBoardDevices(DeviceManager& devices, platform::PlatformServices const& platformServices,
    std::span<BoardDeviceProvider const> providers) {
	// Проверяем весь список до binding и вызова первого провайдера.
	for (auto provider : providers) {
		if (provider == nullptr) {
			return false;
		}
	}

	auto const& activeBoard = detail::boardFor(platformServices);
	if (!devices.bindBoardGpio(activeBoard.userGpio, platformServices.gpio)) {
		return false;
	}

	for (auto provider : providers) {
		provider(devices, platformServices, activeBoard);
	}

	// One arbiter for each *physical* bus. Name aliases share the same object.
	for (std::size_t index = 0; index < devices.i2cBusCount(); ++index) {
		auto* const bus = devices.i2cBusAt(index).bus;
		if (bus != nullptr) {
			bus->setTransactionWaitFunction(platformServices.clock.sleepMs);
		}
	}

	for (std::size_t index = 0; index < devices.spiBusCount(); ++index) {
		if (auto* bus = devices.spiBusAt(index).bus; bus != nullptr) {
			bus->setTransactionWaitFunction(platformServices.clock.sleepMs);
		}
	}
#if FEATURE_ENABLED(PICO_CALC)
	// The STM32 keyboard/southbridge is system-owned. Reserve the address,
	// not the whole controller, so user peripherals on other addresses work.
	if (devices.picoCalcSouthbridgeServiceCount() != 0) {
		(void) devices.reserveI2cAddress("debug", 0x1FU, "picocalc-keyboard");
	}
#endif
	return true;
}

} // namespace lexxos::composition
