#include <lexxos/platform/board.h>

namespace lexxos::platform {

BoardDescriptor const& activeBoardDescriptor() {
	static BoardDescriptor const descriptor{
	    .boardId = "m5stack_basic_v1",
	    .boardName = "M5Stack Basic Core v1",
	    .vendor = "M5Stack",
	    .processorFamily = ProcessorFamily::Esp32,
	    .processorName = "ESP32-D0WDQ6",

	    // У раннего M5Stack Basic нет отдельного пользовательского status LED.
	    .statusLed = {invalidPin},
	    .statusLedActiveLevel = {},

	    .debugUart = {
	                  .index = 0,
	                  .tx = {1},
	                  .rx = {3},
	                  .baudRate = 115200,
	                  },

	    // Grove/I2C bus. В первом патче оставлен как descriptor; драйвер I2C будет отдельным шагом.
	    .debugOledI2c = {
	                  .index = 0,
	                  .sda = {21},
	                  .scl = {22},
	                  .baudRate = 400000,
	                  },

	    // Встроенная TF/microSD карта M5Stack Basic подключена к общей SPI-шине LCD/TF.
	    // Драйвер SD намеренно не включён в первом bring-up патче.
	    .sdInterface = SdInterface::Spi,
	    .sdSpi = {
	                  .index = 1,
	                  .sck = {18},
	                  .mosi = {23},
	                  .miso = {19},
	                  .cs = {4},
	                  .baudRate = 20000000,
	                  },
	    .sdSdio = {},
	    .sdCardDetect = {},
	    .psram = {},
	    // Встроенный LCD M5Stack Basic совместим с ILI9341/ILI9342C-командным набором.
	    // Структура spiTftDisplay используется как общий SPI RGB565 TFT descriptor.
	    .spiTftDisplay = {
	                  .enabled = true,
	                  .spiIndex = 1,
	                  .spiBaudRate = 40000000,
	                  .sck = {18},
	                  .mosi = {23},
	                  .cs = {14},
	                  .dc = {27},
	                  .reset = {33},
	                  .backlight = {32},
	                  .width = 320,
	                  .height = 240,
	                  .columnOffset = 0,
	                  .rowOffset = 0,
	                  .invertColors = false,
	                  .controller = SpiTftController::Ili934x,
	                  .madctl = 0x28,
	                  .addressWindowSwapsAxes = true,
	                  },
	    .keyboardExpander = {},
	    .parallelTftDisplay = {},
	    .keyboard = {},
	    .segaGamepad = {},

	    // Кнопки будут добавлены отдельным DirectButtons input device.
	    // Button A=39, Button B=38, Button C=37, active low.
	};

	return descriptor;
}

} // namespace lexxos::platform
