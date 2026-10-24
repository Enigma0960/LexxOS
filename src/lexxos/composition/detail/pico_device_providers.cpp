#include <lexxos/composition/detail/platform_device_providers.h>

#include <iterator>
#include <string_view>

#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/feature.h>
#include <lexxos/platform/platform.h>
#include <lexxos/platform/platform_services.h>

#if FEATURE_ENABLED(DISPLAY)
	#include <lexxos/devices/display/st7735_display.h>
#endif
#if defined(LEXXOS_PLATFORM_PICO) && FEATURE_ENABLED(DISPLAY) && FEATURE_ENABLED(PICO_CALC)
	#include <lexxos/devices/display/ili9488_display.h>
#endif
#if FEATURE_ENABLED(KEYBOARD)
	#include <lexxos/devices/input/keyboard_gamepad_adapter.h>
	#include <lexxos/devices/input/pcf8575_matrix_keyboard.h>
	#include <lexxos/devices/input/sega_genesis_gamepad.h>
	#include <lexxos/devices/io/pcf8575.h>
#endif
#if defined(LEXXOS_PLATFORM_PICO) && FEATURE_ENABLED(PICO_CALC)
	#include <lexxos/devices/input/keyboard_gamepad_adapter.h>
	#include <lexxos/devices/input/picocalc_keyboard.h>
	#include <lexxos/devices/picocalc/picocalc_southbridge_resource_policy.h>
#endif

namespace lexxos::composition::detail {

using lexxos::devices::debugI2cBus;
using lexxos::devices::DeviceManager;
using lexxos::devices::I2cBus;
#if defined(LEXXOS_PLATFORM_PICO) && FEATURE_ENABLED(DISPLAY)
using lexxos::devices::St7735Display;
#endif
#if defined(LEXXOS_PLATFORM_PICO) && FEATURE_ENABLED(DISPLAY) && FEATURE_ENABLED(PICO_CALC)
using lexxos::devices::Ili9488Display;
#endif
#if defined(LEXXOS_PLATFORM_PICO) && FEATURE_ENABLED(KEYBOARD)
using lexxos::devices::KeyCode;
using lexxos::devices::Pcf8575MatrixKeyboard;
using lexxos::devices::Pcf8575PortExpander;
using lexxos::devices::SegaGenesisGamepad;
#endif
#if defined(LEXXOS_PLATFORM_PICO) && FEATURE_ENABLED(PICO_CALC) && !FEATURE_ENABLED(KEYBOARD)
using lexxos::devices::KeyCode;
#endif
#if defined(LEXXOS_PLATFORM_PICO) && FEATURE_ENABLED(PICO_CALC)
using lexxos::devices::KeyboardGamepadAdapter;
using lexxos::devices::PicoCalcKeyboard;
using lexxos::devices::PicoCalcSouthbridgeResourcePolicy;
#endif
#if (defined(LEXXOS_PLATFORM_PICO) && FEATURE_ENABLED(KEYBOARD) && !FEATURE_ENABLED(PICO_CALC)) \
    || (defined(LEXXOS_PLATFORM_SIM) && FEATURE_ENABLED(KEYBOARD))
using lexxos::devices::KeyboardGamepadAdapter;
#endif

namespace {
#if defined(LEXXOS_PLATFORM_PICO) && FEATURE_ENABLED(DISPLAY) && FEATURE_ENABLED(PICO_CALC)
	bool isPicoCalcBoard(lexxos::platform::BoardDescriptor const& board) {
		return std::string_view(board.boardId) == "picocalc_pico"
		    || std::string_view(board.boardId) == "picocalc_pico_w"
		    || std::string_view(board.boardId) == "picocalc_pico2"
		    || std::string_view(board.boardId) == "picocalc_pico2w";
	}
#endif
#if defined(LEXXOS_PLATFORM_PICO) && FEATURE_ENABLED(DISPLAY)
	St7735Display& boardTftDisplay(lexxos::platform::PlatformServices const& platform) {
		static St7735Display display{lexxos::platform::effectiveSpiTftDisplay(boardFor(platform)), platform};
		return display;
	}
#endif
#if defined(LEXXOS_PLATFORM_PICO) && FEATURE_ENABLED(DISPLAY) && FEATURE_ENABLED(PICO_CALC)
	Ili9488Display& boardPicoCalcDisplay(lexxos::platform::PlatformServices const& platform) {
		static Ili9488Display display{lexxos::platform::effectiveSpiTftDisplay(boardFor(platform)), platform};
		return display;
	}
#endif
#if defined(LEXXOS_PLATFORM_PICO) && FEATURE_ENABLED(KEYBOARD)
	constexpr KeyCode devPicoKeyboardMap[24]{
	    KeyCode::Num3, // index 0  r0c0
	    KeyCode::Num1, // index 1  r0c1
	    KeyCode::Num2, // index 2  r0c2
	    KeyCode::Enter, // index 3  r0c3, next/ok
	    KeyCode::Sharp, // index 4  r1c0
	    KeyCode::Star, // index 5  r1c1
	    KeyCode::Num0, // index 6  r1c2
	    KeyCode::Up, // index 7  r1c3
	    KeyCode::Unknown, // index 8  r2c0
	    KeyCode::Unknown, // index 9  r2c1
	    KeyCode::Unknown, // index 10 r2c2, board-specific always-on position
	    KeyCode::Unknown, // index 11 r2c3
	    KeyCode::Down, // index 12 r3c0
	    KeyCode::Unknown, // index 13 r3c1
	    KeyCode::Unknown, // index 14 r3c2
	    KeyCode::Escape, // index 15 r3c3, back/cancel
	    KeyCode::Num6, // index 16 r4c0
	    KeyCode::Num4, // index 17 r4c1
	    KeyCode::Num5, // index 18 r4c2
	    KeyCode::Left, // index 19 r4c3
	    KeyCode::Num9, // index 20 r5c0
	    KeyCode::Num7, // index 21 r5c1
	    KeyCode::Num8, // index 22 r5c2
	    KeyCode::Right, // index 23 r5c3
	};

	constexpr std::uint32_t devPicoKeyboardIgnoredMask = 1U << 10;

	Pcf8575PortExpander& boardKeyboardExpander(lexxos::platform::PlatformServices const& platform) {
		static Pcf8575PortExpander expander{debugI2cBus(), boardFor(platform).keyboardExpander};
		return expander;
	}

	Pcf8575MatrixKeyboard& boardKeyboard(lexxos::platform::PlatformServices const& platform) {
		auto const& keyboard = boardFor(platform).keyboard;
		static Pcf8575MatrixKeyboard device{
		    boardKeyboardExpander(platform),
		    {
		                .name = keyboard.name,
		                .rows = keyboard.rows,
		                .rowCount = keyboard.rowCount,
		                .columns = keyboard.columns,
		                .columnCount = keyboard.columnCount,
		                .keyMap = devPicoKeyboardMap,
		                .keyMapCount = std::size(devPicoKeyboardMap),
		                .ignoredKeyMask = devPicoKeyboardIgnoredMask,
		                .rowActiveLevel = keyboard.rowActiveLevel,
		                .columnActiveLevel = keyboard.columnActiveLevel,
		                .scanPeriodMs = keyboard.scanPeriodMs,
		                .debounceMs = keyboard.debounceMs,
		                },
		    platform,
		};
		return device;
	}

	KeyboardGamepadAdapter& boardKeyboardGamepad(lexxos::platform::PlatformServices const& platform) {
		static KeyboardGamepadAdapter gamepad{
		    "main-gamepad",
		    boardKeyboard(platform),
		    {
		                .up = KeyCode::Up,
		                .down = KeyCode::Down,
		                .left = KeyCode::Left,
		                .right = KeyCode::Right,
		                .a = KeyCode::Num1,
		                .b = KeyCode::Num3,
		                .start = KeyCode::Enter,
		                .select = KeyCode::Escape,
		                },
		};
		return gamepad;
	}

	SegaGenesisGamepad& boardSegaGamepad(lexxos::platform::PlatformServices const& platform) {
		static SegaGenesisGamepad gamepad{boardKeyboardExpander(platform), boardFor(platform).segaGamepad, platform};
		return gamepad;
	}
#endif
#if defined(LEXXOS_PLATFORM_PICO) && FEATURE_ENABLED(PICO_CALC)
	PicoCalcKeyboard& picoCalcKeyboard(lexxos::platform::PlatformServices const& platform) {
		static PicoCalcKeyboard keyboard{debugI2cBus(), platform};
		return keyboard;
	}
#endif
#if defined(LEXXOS_PLATFORM_PICO) && FEATURE_ENABLED(PICO_CALC)
	KeyboardGamepadAdapter& picoCalcKeyboardGamepad(lexxos::platform::PlatformServices const& platform) {
		static KeyboardGamepadAdapter gamepad{
		    "picocalc-gamepad",
		    picoCalcKeyboard(platform),
		    {
		                .up = KeyCode::Up,
		                .down = KeyCode::Down,
		                .left = KeyCode::Left,
		                .right = KeyCode::Right,
		                .a = KeyCode::I,
		                .b = KeyCode::O,
		                .x = KeyCode::K,
		                .y = KeyCode::L,
		                .start = KeyCode::Tab,
		                .select = KeyCode::Backspace,
		                },
		    // PicoCalc keyboard is already updated by the picocalc-input task; avoid a blocking I2C poll per emulated frame.
		    false,
		};
		return gamepad;
	}
#endif
} // namespace

void registerPicoBoardDevices(
    DeviceManager& devices,
    lexxos::platform::PlatformServices const& platformServices,
    lexxos::platform::BoardDescriptor const& board) {
	(void) devices;
	(void) platformServices;
	(void) board;
#if defined(LEXXOS_PLATFORM_PICO) && FEATURE_ENABLED(DISPLAY)
	auto const displayPins = lexxos::platform::effectiveSpiTftDisplay(board);
	if (displayPins.isValid()) {
	#if FEATURE_ENABLED(PICO_CALC)
		if (isPicoCalcBoard(board)) {
			auto& display = boardPicoCalcDisplay(platformServices);
			display.initialize();
			devices.registerDisplayDevice(display.name(), "PicoCalc ILI9488 320x320 RGB display", display);
		} else
	#endif
		{
			auto& display = boardTftDisplay(platformServices);
			display.initialize();
			devices.registerDisplayDevice(display.name(), "ST7735S 128x160 RGB565 TFT display", display);
		}
	}
#endif

#if defined(LEXXOS_PLATFORM_PICO) && FEATURE_ENABLED(KEYBOARD)
	if (board.keyboardExpander.isValid() && board.keyboard.isValid()) {
		auto& expander = boardKeyboardExpander(platformServices);
		expander.initialize();
		auto& keyboard = boardKeyboard(platformServices);
		keyboard.initialize();
		devices.registerInputDevice(keyboard.name(), "4x5 matrix keyboard through PCF8575", keyboard);
		auto& gamepad = boardKeyboardGamepad(platformServices);
		devices.registerGamepadDevice(gamepad.name(), "keyboard gamepad adapter", gamepad);

		if (board.segaGamepad.isValid()) {
			auto& segaGamepad = boardSegaGamepad(platformServices);
			segaGamepad.initialize();
			devices.registerGamepadDevice(segaGamepad.name(), "Sega Mega Drive/Genesis 3-button controller", segaGamepad);
		}
	}
#endif

#if defined(LEXXOS_PLATFORM_PICO) && FEATURE_ENABLED(PICO_CALC)
	auto& picoCalcInput = picoCalcKeyboard(platformServices);
	picoCalcInput.initialize();
	devices.registerInputDevice(picoCalcInput.name(), "PicoCalc STM32 southbridge keyboard", picoCalcInput);
	auto& picoCalcGamepad = picoCalcKeyboardGamepad(platformServices);
	devices.registerGamepadDevice(picoCalcGamepad.name(), "PicoCalc keyboard gamepad adapter", picoCalcGamepad);
	devices.registerPicoCalcSouthbridgeService(
	    PicoCalcSouthbridgeResourcePolicy::serviceName,
	    "PicoCalc STM32 southbridge runtime service",
	    picoCalcInput.southbridgeRuntimeService());
#endif
}

} // namespace lexxos::composition::detail
