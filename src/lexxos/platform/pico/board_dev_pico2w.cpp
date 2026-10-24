#include <lexxos/platform/board.h>

namespace lexxos::platform {

BoardDescriptor const& activeBoardDescriptor() {
	static BoardDescriptor const descriptor{
	    .boardId = "dev_pico2w",
	    .boardName = "LexxOS development board on Raspberry Pi Pico 2 W",
	    .vendor = "LexxOS",
	    .processorFamily = ProcessorFamily::Rp2350,
	    .processorName = "RP2350",
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
	                  .baudRate = 1000000,
	                  },
	    .sdCardDetect = {
	                  .detect = {-1},
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
	    .wireless = {
	                  .wifi = true,
	                  .bluetooth = true,
	                  .radio = "CYW43",
	                  .interfaceName = "wlan0",
	                  .backend = "pico-cyw43",
	                  .autoConnect = false,
	                  .scan = true,
	                  },
	};

	return descriptor;
}

} // namespace lexxos::platform
