#include <lexxos/composition/detail/platform_device_providers.h>

#include <iterator>
#include <string_view>

#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/feature.h>
#include <lexxos/platform/platform.h>
#include <lexxos/platform/platform_services.h>

#if defined(LEXXOS_PLATFORM_SIM) && FEATURE_ENABLED(DISPLAY)
	#include <sim_display_device.h>
#endif
#if defined(LEXXOS_PLATFORM_SIM) && FEATURE_ENABLED(KEYBOARD)
	#include <sim_input_device.h>
#endif
#if FEATURE_ENABLED(KEYBOARD)
	#include <lexxos/devices/input/keyboard_gamepad_adapter.h>
#endif

namespace lexxos::composition::detail {

using lexxos::devices::debugI2cBus;
using lexxos::devices::DeviceManager;
using lexxos::devices::I2cBus;
#if (defined(LEXXOS_PLATFORM_PICO) && FEATURE_ENABLED(KEYBOARD) && !FEATURE_ENABLED(PICO_CALC)) \
    || (defined(LEXXOS_PLATFORM_SIM) && FEATURE_ENABLED(KEYBOARD))
using lexxos::devices::KeyboardGamepadAdapter;
#endif

namespace {
} // namespace

void registerSimulatorDevices(DeviceManager& devices) {
	(void) devices;
#if defined(LEXXOS_PLATFORM_SIM) && FEATURE_ENABLED(DISPLAY)
	auto& simDisplay = lexxos::emulator::simDisplayDevice();
	devices.registerDisplayDevice(simDisplay.name(), "emulated RGB565 display", simDisplay);
#endif

#if defined(LEXXOS_PLATFORM_SIM) && FEATURE_ENABLED(KEYBOARD)
	auto& simInput = lexxos::emulator::simInputDevice();
	static KeyboardGamepadAdapter simGamepad{"sim-gamepad", simInput};
	devices.registerInputDevice(simInput.name(), "simulated keyboard input", simInput);
	devices.registerGamepadDevice(simGamepad.name(), "simulated keyboard gamepad adapter", simGamepad);
#endif
}

} // namespace lexxos::composition::detail
