#include <lexxos/platform/board.h>

namespace lexxos::platform {

#ifndef LEXXOS_ST7735_SPI_BAUDRATE
	#define LEXXOS_ST7735_SPI_BAUDRATE 25000000
#endif

BoardDescriptor const& activeBoardDescriptor() {
	static BoardDescriptor const descriptor{
	    .boardId = "dev_pico",
	    .boardName = "LexxOS development board on Raspberry Pi Pico",
	    .vendor = "LexxOS",
	    .processorFamily = ProcessorFamily::Rp2040,
	    .processorName = "RP2040",
	    .statusLed = {25},
	    .debugUart = {
	                  .index = 0,
	                  .tx = {0},
	                  .rx = {1},
	                  .baudRate = 115200,
	                  },
	    .debugOledI2c = {
	                  .index = 0,
	                  .sda = {20},
	                  .scl = {21},
	                  .baudRate = 400000,
	                  },
	    .sdInterface = SdInterface::Spi,
	    .sdSpi = {
	                  .index = 0,
	                  .sck = {18},
	                  .mosi = {19},
	                  .miso = {16},
	                  .cs = {17},
	                  .baudRate = 50000000,
	                  },
	    .sdCardDetect = {
	                  .detect = {22},
	                  .insertedLevel = GpioActiveLevel::Low,
	                  },
	    .psram = {
	                  .spi = {
	            .index = 1,
	            .sck = {10},
	            .mosi = {11},
	            .miso = {12},
	            .cs = {9},
	            .baudRate = 1000000,
	        },
	                  .sio2 = {13},
	                  .sio3 = {14},
	                  },
	    .spiTftDisplay = {
	                  .enabled = true,
	                  .spiIndex = 1,
	                  .spiBaudRate = LEXXOS_ST7735_SPI_BAUDRATE,
	                  .sck = {10},
	                  .mosi = {11},
	                  .cs = {5},
	                  .dc = {4},
	                  .reset = {6},
	                  .backlight = {7},
	                  .width = 128,
	                  .height = 160,
	                  .columnOffset = 0,
	                  .rowOffset = 0,
	                  .invertColors = false,
	                  },
	    .keyboardExpander = {
	                  .enabled = true,
	                  .name = "kbd-pcf8575",
	                  .i2cIndex = 0,
	                  .address = 0x20,
	                  .interrupt = {27},
	                  .interruptActiveLevel = GpioActiveLevel::Low,
	                  },
	    .keyboard = {
	                  .enabled = true,
	                  .name = "main-keyboard",
	                  .expanderName = "kbd-pcf8575",
	                  //.rows = {0, 1, 2, 3},
	        .rows = {8, 9, 10, 11, 12, 13},
	                  .rowCount = 6,
	                  // .columns = {8, 9, 10, 11, 12},
	        .columns = {0, 1, 2, 3},
	                  .columnCount = 4,
	                  .rowActiveLevel = GpioActiveLevel::Low,
	                  .columnActiveLevel = GpioActiveLevel::Low,
	                  .scanPeriodMs = 5,
	                  .debounceMs = 20,
	                  },
	    .segaGamepad = {
	                  .enabled = true,
	                  .name = "sega-gamepad",
	                  .expanderName = "kbd-pcf8575",
	                  .up = 4,
	                  .down = 5,
	                  .left = 6,
	                  .right = 7,
	                  .buttonBa = 14,
	                  .buttonCStart = 15,
	                  .select = {2},
	                  },
	};

	return descriptor;
}

} // namespace lexxos::platform
