#include <lexxos/platform/board.h>

namespace lexxos::platform {

BoardDescriptor const& activeBoardDescriptor() {
	static BoardDescriptor const descriptor{
	    .boardId = "dev_pico_sdio",
	    .boardName = "LexxOS development board on Raspberry Pi Pico (experimental SDIO profile)",
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
	    .sdInterface = SdInterface::Sdio4Bit,
	    .sdSdio = {
	                  .index = 0,
	                  .clock = {21},
	                  .command = {20},
	                  .data0 = {16},
	                  .data1 = {17},
	                  .data2 = {18},
	                  .data3 = {19},
	                  .initializationClockHz = 400000,
	                  .transferClockHz = 24000000,
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
	                  .sck = {2},
	                  .mosi = {3},
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
	};

	return descriptor;
}

} // namespace lexxos::platform
