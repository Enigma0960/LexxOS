#include <lexxos/platform/board.h>

namespace lexxos::platform {

#ifndef LEXXOS_CARDPUTER_LCD_SPI_BAUDRATE
	#define LEXXOS_CARDPUTER_LCD_SPI_BAUDRATE 40000000
#endif
#ifndef LEXXOS_CARDPUTER_LCD_MADCTL
	#define LEXXOS_CARDPUTER_LCD_MADCTL 0x60
#endif
#ifdef LEXXOS_CARDPUTER_LCD_ADDRESS_WINDOW_SWAPS_AXES
	#define LEXXOS_CARDPUTER_LCD_SWAP_AXES true
#else
	#define LEXXOS_CARDPUTER_LCD_SWAP_AXES false
#endif

BoardDescriptor const& activeBoardDescriptor() {
	static BoardDescriptor const descriptor{
	    .boardId = "m5stack_cardputer",
	    .boardName = "M5Stack Cardputer",
	    .vendor = "M5Stack",
	    .processorFamily = ProcessorFamily::Esp32,
	    .processorName = "ESP32-S3FN8",

	    .statusLed = {invalidPin},
	    .statusLedActiveLevel = {},

	    // Cardputer/StampS3 exposes the normal development console through USB
	    // Serial/JTAG. The first standalone bring-up relies on LCD console mirror
	    // and the built-in keyboard instead of a UART-backed shell.
	    .debugUart = {
	                  .index = 0,
	                  .tx = {43},
	                  .rx = {44},
	                  .baudRate = 115200,
	                  },

	    .interfaces = {
	                  .consoleUart = {
	            .index = 0,
	            .tx = {43},
	            .rx = {44},
	            .baudRate = 115200,
	        },
	                  .systemI2c = {
	            .index = 0,
	            .sda = {2},
	            .scl = {1},
	            .baudRate = 400000,
	        },
	                  .storageSpi = {
	            .index = 0,
	            .sck = {40},
	            .mosi = {14},
	            .miso = {39},
	            .baudRate = 20000000,
	        },
	                  .displaySpi = {
	            .index = 1,
	            .sck = {36},
	            .mosi = {35},
	            .baudRate = LEXXOS_CARDPUTER_LCD_SPI_BAUDRATE,
	        },
	                  },

	    // Declared for future SD support. The Cardputer first patch disables
	    // SD/FAT at CMake level because LCD and microSD do not share one SPI bus.
	    .sdInterface = SdInterface::Spi,
	    .sdSpi = {
	                  .index = 0,
	                  .sck = {40},
	                  .mosi = {14},
	                  .miso = {39},
	                  .cs = {12},
	                  .baudRate = 20000000,
	                  },
	    .sdSdio = {},
	    .sdCardDetect = {},
	    .psram = {},
	    .wireless = {
	                  .wifi = true,
	                  .bluetooth = true,
	                  .radio = "ESP32-S3",
	                  .interfaceName = "wlan0",
	                  .backend = "esp32-native",
	                  .autoConnect = false,
	                  .scan = true,
	                  },
	    .spiTftDisplay = {
	                  .enabled = true,
	                  .spiIndex = 1,
	                  .spiBaudRate = LEXXOS_CARDPUTER_LCD_SPI_BAUDRATE,
	                  .sck = {36},
	                  .mosi = {35},
	                  .cs = {37},
	                  .dc = {34},
	                  .reset = {33},
	                  .backlight = {38},
	                  .width = 240,
	                  .height = 135,
	                  .columnOffset = 40,
	                  .rowOffset = 52,
	                  .invertColors = true,
	                  .controller = SpiTftController::St7789,
	                  .madctl = LEXXOS_CARDPUTER_LCD_MADCTL,
	                  .addressWindowSwapsAxes = LEXXOS_CARDPUTER_LCD_SWAP_AXES,
	                  },
	};

	return descriptor;
}

} // namespace lexxos::platform
