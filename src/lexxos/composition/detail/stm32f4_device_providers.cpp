#include <lexxos/composition/detail/platform_device_providers.h>

#include <iterator>
#include <string_view>

#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/feature.h>
#include <lexxos/platform/platform.h>
#include <lexxos/platform/platform_services.h>

#if defined(LEXXOS_PLATFORM_STM32F4) && FEATURE_ENABLED(DISPLAY)
	#include <lexxos/devices/stm32/parallel_tft_display_stm32.h>
#endif

namespace lexxos::composition::detail {

using lexxos::devices::debugI2cBus;
using lexxos::devices::DeviceManager;
using lexxos::devices::I2cBus;
#if defined(LEXXOS_PLATFORM_STM32F4) && FEATURE_ENABLED(DISPLAY)
using lexxos::devices::Stm32ParallelTftDisplay;
#endif

namespace {
#if defined(LEXXOS_PLATFORM_STM32F4) && FEATURE_ENABLED(DISPLAY)
	Stm32ParallelTftDisplay& boardParallelTftDisplay(lexxos::platform::PlatformServices const& platform) {
		static Stm32ParallelTftDisplay display{boardFor(platform).parallelTftDisplay, platform};
		return display;
	}
#endif
} // namespace

void registerStm32BoardDevices(
    DeviceManager& devices,
    lexxos::platform::PlatformServices const& platformServices,
    lexxos::platform::BoardDescriptor const& activeBoard) {
	(void) devices;
	(void) platformServices;
	(void) activeBoard;
#if defined(LEXXOS_PLATFORM_STM32F4) && FEATURE_ENABLED(DISPLAY)
	if (auto const& displayPins = activeBoard.parallelTftDisplay; displayPins.isValid()) {
		auto& display = boardParallelTftDisplay(platformServices);
		display.initialize();
		devices.registerDisplayDevice(display.name(), "EU-TFT3.2 320x240 RGB565 parallel TFT display", display);
	}
#endif
}

} // namespace lexxos::composition::detail
