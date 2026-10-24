#include <lexxos/composition/detail/platform_device_providers.h>

#include <iterator>
#include <string_view>

#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/input_device.h>
#include <lexxos/feature.h>
#include <lexxos/platform/platform.h>
#include <lexxos/platform/platform_services.h>

#if defined(LEXXOS_PLATFORM_ESPIDF) && FEATURE_ENABLED(DISPLAY)
	#include <lexxos/devices/espidf/m5stack_lcd_display.h>
	#include <lexxos/devices/espidf/mipi_dsi_display_espidf.h>
#endif
#if defined(LEXXOS_PLATFORM_ESPIDF) && FEATURE_ENABLED(KEYBOARD) && !FEATURE_ENABLED(CARDPUTER) && (LEXXOS_BOARD_FAMILY_JC_ESP32P4_M3_DEV != 1)
	#include <lexxos/devices/espidf/m5stack_buttons_input_device.h>
#endif
#if defined(LEXXOS_PLATFORM_ESPIDF) && (LEXXOS_BOARD_FAMILY_JC_ESP32P4_M3_DEV == 1)
	#include <lexxos/devices/espidf/usb_hid_host.h>
	#include <lexxos/devices/gamepad_device.h>
	#include <lexxos/devices/pointing_device.h>
	#include <lexxos/devices/touch/ft5x06_touch_device.h>
	#include <lexxos/devices/touch/gt911_touch_device.h>
#endif
#if defined(LEXXOS_PLATFORM_ESPIDF) && FEATURE_ENABLED(CARDPUTER)
	#include <lexxos/devices/espidf/cardputer_ir_transmitter.h>
	#include <lexxos/devices/espidf/cardputer_keyboard.h>
	#include <lexxos/devices/espidf/cardputer_tca8418_keyboard.h>
	#include <lexxos/devices/sensors/bmi270.h>
#endif
#if FEATURE_ENABLED(CARDPUTER)
	#include <lexxos/devices/input/keyboard_gamepad_adapter.h>
#endif

namespace lexxos::composition::detail {

using lexxos::devices::debugI2cBus;
using lexxos::devices::DeviceManager;
using lexxos::devices::I2cBus;
#if defined(LEXXOS_PLATFORM_ESPIDF) && FEATURE_ENABLED(DISPLAY)
using lexxos::devices::displaySpiBus;
using lexxos::devices::EspIdfMipiDsiDisplay;
using lexxos::devices::M5StackLcdDisplay;
#endif
#if defined(LEXXOS_PLATFORM_ESPIDF) && FEATURE_ENABLED(DISPLAY) && (LEXXOS_BOARD_FAMILY_JC_ESP32P4_M3_DEV == 1)
using lexxos::devices::Ft5x06TouchDevice;
using lexxos::devices::Gt911TouchDevice;
#endif
#if defined(LEXXOS_PLATFORM_ESPIDF) && FEATURE_ENABLED(KEYBOARD) && !FEATURE_ENABLED(CARDPUTER) && (LEXXOS_BOARD_FAMILY_JC_ESP32P4_M3_DEV != 1)
using lexxos::devices::M5StackButtonsInputConfig;
using lexxos::devices::M5StackButtonsInputDevice;
#endif
#if defined(LEXXOS_PLATFORM_ESPIDF) && FEATURE_ENABLED(CARDPUTER)
using lexxos::devices::Bmi270Sensor;
using lexxos::devices::CardputerIrTransmitter;
using lexxos::devices::CardputerKeyboard;
using lexxos::devices::CardputerTca8418Keyboard;
using lexxos::devices::KeyboardGamepadAdapter;
using lexxos::devices::KeyCode;
#endif

namespace {
#if defined(LEXXOS_PLATFORM_ESPIDF) && FEATURE_ENABLED(CARDPUTER)
	bool isCardputerAdvBoard(lexxos::platform::BoardDescriptor const& board) {
		return std::string_view(board.boardId) == "m5stack_cardputer_adv";
	}
#endif
#if defined(LEXXOS_PLATFORM_ESPIDF) && FEATURE_ENABLED(DISPLAY)
	M5StackLcdDisplay& boardM5StackLcdDisplay(lexxos::platform::PlatformServices const& platform) {
		static M5StackLcdDisplay display{displaySpiBus(), boardFor(platform).spiTftDisplay, platform};
		return display;
	}

#endif
#if defined(LEXXOS_PLATFORM_ESPIDF) && FEATURE_ENABLED(DISPLAY) && (LEXXOS_BOARD_FAMILY_JC_ESP32P4_M3_DEV == 1)
	EspIdfMipiDsiDisplay& boardMipiDsiDisplay(I2cBus& controlBus, lexxos::platform::PlatformServices const& platform) {
		static EspIdfMipiDsiDisplay display{controlBus, boardFor(platform).mipiDsiDisplay};
		return display;
	}
#endif
#if defined(LEXXOS_PLATFORM_ESPIDF) && FEATURE_ENABLED(DISPLAY) && (LEXXOS_BOARD_FAMILY_JC_ESP32P4_M3_DEV == 1)
	Gt911TouchDevice& boardGt911Touch(I2cBus& bus, lexxos::platform::PlatformServices const& platform) {
		static Gt911TouchDevice touch{bus, boardFor(platform).touchPanel, platform};
		return touch;
	}

	Ft5x06TouchDevice& boardFt5x06Touch(I2cBus& bus, lexxos::platform::PlatformServices const& platform) {
		static Ft5x06TouchDevice touch{bus, boardFor(platform).touchPanel, platform};
		return touch;
	}
#endif
#if defined(LEXXOS_PLATFORM_ESPIDF) && FEATURE_ENABLED(CARDPUTER)
	CardputerKeyboard& cardputerMatrixKeyboard(lexxos::platform::PlatformServices const& platform) {
		static CardputerKeyboard keyboard{platform};
		return keyboard;
	}

	CardputerTca8418Keyboard& cardputerAdvKeyboard(I2cBus& bus) {
		static CardputerTca8418Keyboard keyboard{bus};
		return keyboard;
	}

	KeyboardGamepadAdapter& cardputerAdvKeyboardGamepad(I2cBus& bus) {
		static KeyboardGamepadAdapter gamepad{
		    "cardputer-gamepad",
		    cardputerAdvKeyboard(bus),
		    {
		           .up = KeyCode::E,
		           .down = KeyCode::Z,
		           .left = KeyCode::A,
		           .right = KeyCode::D,
		           .a = KeyCode::LeftBracket,
		           .b = KeyCode::RightBracket,
		           .x = KeyCode::Semicolon,
		           .y = KeyCode::Grave,
		           .start = KeyCode::Enter,
		           .select = KeyCode::Backspace,
		           },
		};
		return gamepad;
	}

	Bmi270Sensor& cardputerBmi270(I2cBus& bus, lexxos::platform::PlatformServices const& platform) {
		static Bmi270Sensor sensor{bus, boardFor(platform).motionSensor, platform};
		return sensor;
	}

	CardputerIrTransmitter& cardputerIr(lexxos::platform::PlatformServices const& platform) {
		static CardputerIrTransmitter ir{boardFor(platform).infraredTransmitter, platform};
		return ir;
	}
#endif
#if defined(LEXXOS_PLATFORM_ESPIDF) && FEATURE_ENABLED(KEYBOARD) && !FEATURE_ENABLED(CARDPUTER) && (LEXXOS_BOARD_FAMILY_JC_ESP32P4_M3_DEV != 1)
	M5StackButtonsInputDevice& m5stackButtonsInputDevice(lexxos::platform::PlatformServices const& platform) {
		static M5StackButtonsInputDevice device{
		    M5StackButtonsInputConfig{
		                              .name = "m5stack-buttons",
		                              },
		    platform,
		};
		return device;
	}
#endif
} // namespace

void registerEspidfBoardDevices(
    DeviceManager& devices,
    lexxos::platform::PlatformServices const& platformServices,
    lexxos::platform::BoardDescriptor const& activeBoard,
    I2cBus& debugBus) {
	(void) devices;
	(void) platformServices;
	(void) activeBoard;
	(void) debugBus;
#if defined(LEXXOS_PLATFORM_ESPIDF) && FEATURE_ENABLED(DISPLAY)
	#if LEXXOS_BOARD_FAMILY_JC_ESP32P4_M3_DEV == 1
	if (auto const& dsiDisplay = activeBoard.mipiDsiDisplay; dsiDisplay.isValid()) {
		auto& display = boardMipiDsiDisplay(debugBus, platformServices);
		display.initialize();
		devices.registerDisplayDevice(display.name(), "BTT Pi TFT70 800x480 MIPI DSI RGB display", display);
	}

	if (auto const& touchPanel = activeBoard.touchPanel; touchPanel.isValid()) {
		switch (touchPanel.controller) {
			case lexxos::platform::TouchPanelKind::Gt911: {
				auto& touch = boardGt911Touch(debugBus, platformServices);
				if (touch.initialize()) {
					devices.registerPointingDevice(touch.name(), "GT911 capacitive multi-touch panel", touch);
				} else if (platformServices.console.write != nullptr) {
					platformServices.console.write("gt911-touch: initialize failed\r\n");
				}
				break;
			}
			case lexxos::platform::TouchPanelKind::FocalTechFt5x06: {
				auto& touch = boardFt5x06Touch(debugBus, platformServices);
				if (touch.initialize()) {
					devices.registerPointingDevice(touch.name(), "FocalTech FT5x06 capacitive multi-touch panel", touch);
				} else if (platformServices.console.write != nullptr) {
					platformServices.console.write("ft5x06-touch: initialize failed\r\n");
				}
				break;
			}
			case lexxos::platform::TouchPanelKind::None:
				break;
		}
	}
	#endif

	if (auto const& displayPins = activeBoard.spiTftDisplay; displayPins.isValid()) {
		auto& display = boardM5StackLcdDisplay(platformServices);
		display.initialize();
	#if FEATURE_ENABLED(CARDPUTER)
		devices.registerDisplayDevice(display.name(), "M5Stack Cardputer ST7789V2 240x135 SPI LCD", display);
	#else
		devices.registerDisplayDevice(display.name(), "M5Stack Basic 320x240 SPI LCD", display);
	#endif
	}
#endif

#if defined(LEXXOS_PLATFORM_ESPIDF) && FEATURE_ENABLED(KEYBOARD) && !FEATURE_ENABLED(CARDPUTER) && (LEXXOS_BOARD_FAMILY_JC_ESP32P4_M3_DEV != 1)
	auto& m5stackButtons = m5stackButtonsInputDevice(platformServices);
	if (m5stackButtons.initialize()) {
		devices.registerInputDevice(m5stackButtons.name(), "M5Stack Basic front buttons A/B/C", m5stackButtons);
	}
#endif

#if defined(LEXXOS_PLATFORM_ESPIDF) && (LEXXOS_BOARD_FAMILY_JC_ESP32P4_M3_DEV == 1)
	if (lexxos::devices::espidf_usb_hid::initialize()) {
		auto& keyboard = lexxos::devices::espidf_usb_hid::keyboardDevice();
		devices.registerInputDevice(keyboard.name(), "USB HID boot keyboard through ESP32-P4 USB Host", keyboard);
		auto& mouse = lexxos::devices::espidf_usb_hid::mouseDevice();
		devices.registerPointingDevice(mouse.name(), "USB HID boot mouse through ESP32-P4 USB Host", mouse);
		auto& gamepad = lexxos::devices::espidf_usb_hid::gamepadDevice();
		devices.registerGamepadDevice(gamepad.name(), "USB HID gamepad through ESP32-P4 USB Host", gamepad);
	} else if (platformServices.console.write != nullptr) {
		platformServices.console.write("usb-hid: host initialization failed\r\n");
	}
#endif

#if defined(LEXXOS_PLATFORM_ESPIDF) && FEATURE_ENABLED(CARDPUTER)
	if (isCardputerAdvBoard(activeBoard)) {
		auto& cardputerInput = cardputerAdvKeyboard(debugBus);
		if (cardputerInput.initialize()) {
			devices.registerInputDevice(cardputerInput.name(), "M5Stack Cardputer-Adv TCA8418 56-key keyboard", cardputerInput);
			auto& cardputerGamepad = cardputerAdvKeyboardGamepad(debugBus);
			devices.registerGamepadDevice(cardputerGamepad.name(), "M5Stack Cardputer-Adv keyboard gamepad adapter", cardputerGamepad);
		} else if (platformServices.console.write != nullptr) {
			platformServices.console.write("cardputer-adv keyboard: initialize failed\r\n");
		}
		if (activeBoard.motionSensor.isValid()) {
			auto& motionSensor = cardputerBmi270(debugBus, platformServices);
			if (motionSensor.initialize()) {
				devices.registerSensorDevice(motionSensor.name(), "M5Stack Cardputer-Adv BMI270 6-axis IMU", motionSensor);
			} else if (platformServices.console.write != nullptr) {
				platformServices.console.write("cardputer-adv bmi270: initialize failed\r\n");
			}
		}
		if (activeBoard.infraredTransmitter.isValid()) {
			auto& ir = cardputerIr(platformServices);
			if (ir.initialize()) {
				devices.registerInfraredDevice(ir.name(), "M5Stack Cardputer-Adv IR transmitter", ir);
			} else if (platformServices.console.write != nullptr) {
				platformServices.console.write("cardputer-adv ir: initialize failed\r\n");
			}
		}
	} else {
		auto& cardputerInput = cardputerMatrixKeyboard(platformServices);
		if (cardputerInput.initialize()) {
			devices.registerInputDevice(cardputerInput.name(), "M5Stack Cardputer 56-key matrix keyboard", cardputerInput);
		}
	}
#endif
}

} // namespace lexxos::composition::detail
