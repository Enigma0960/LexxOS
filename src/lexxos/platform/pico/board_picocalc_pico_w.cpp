#include <lexxos/platform/board.h>
#include <lexxos/board_config.h>

namespace lexxos::platform {

#ifndef LEXXOS_PICOCALC_LCD_SPI_BAUDRATE
	#define LEXXOS_PICOCALC_LCD_SPI_BAUDRATE 50000000
#endif

#ifndef LEXXOS_PICOCALC_SD_SPI_BAUDRATE
	#define LEXXOS_PICOCALC_SD_SPI_BAUDRATE 50000000
#endif

#ifndef LEXXOS_PICOCALC_PSRAM_SPI_BAUDRATE
	#define LEXXOS_PICOCALC_PSRAM_SPI_BAUDRATE 8000000
#endif

#ifndef LEXXOS_PICOCALC_SYSTEM_I2C_BAUDRATE
	#if defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
		#define LEXXOS_PICOCALC_SYSTEM_I2C_BAUDRATE 10000
	#else
		#define LEXXOS_PICOCALC_SYSTEM_I2C_BAUDRATE 400000
	#endif
#endif

#ifndef LEXXOS_PICOCALC_AUDIO_BEEP_FREQUENCY_HZ
	#define LEXXOS_PICOCALC_AUDIO_BEEP_FREQUENCY_HZ 880
#endif

#ifndef LEXXOS_PICOCALC_AUDIO_BEEP_DURATION_MS
	#define LEXXOS_PICOCALC_AUDIO_BEEP_DURATION_MS 120
#endif

BoardDescriptor const& activeBoardDescriptor() {
	static BoardDescriptor const descriptor{
#if BOARD_ENABLED(PICOCALC_PICO)
	    .boardId = "picocalc_pico",
	    .boardName = "ClockworkPi PicoCalc with Raspberry Pi Pico",
#else
	    .boardId = "picocalc_pico_w",
	    .boardName = "ClockworkPi PicoCalc with Raspberry Pi Pico W",
#endif
	    .vendor = "ClockworkPi",
	    .processorFamily = ProcessorFamily::Rp2040,
	    .processorName = "RP2040",
	    .statusLed = {-1},
	    .userGpio = {
	                  .pins = {
	            BoardUserGpioPin{
	                .pin = {28},
	                .label = "GP28",
	                .inputAllowed = true,
	                .outputAllowed = true,
	                .defaultPull = GpioPull::Down,
	                .defaultVisible = true,
	            },
	        },
	                  .count = 1,
	                  },
	    .interfaces = {
	                  .consoleUart = {
	            .index = 0,
	            .tx = {0},
	            .rx = {1},
	            .baudRate = 115200,
	        },
	                  .systemI2c = {
	            .index = 1,
	            .sda = {6},
	            .scl = {7},
	            .baudRate = LEXXOS_PICOCALC_SYSTEM_I2C_BAUDRATE,
	        },
	                  .storageSpi = {
	            .index = 0,
	            .sck = {18},
	            .mosi = {19},
	            .miso = {16},
	            .baudRate = LEXXOS_PICOCALC_SD_SPI_BAUDRATE,
	        },
	                  .displaySpi = {
	            .index = 1,
	            .sck = {10},
	            .mosi = {11},
	            .baudRate = LEXXOS_PICOCALC_LCD_SPI_BAUDRATE,
	        },
	                  },
	    .sdInterface = SdInterface::Spi,
	    .sdSpi = {
	                  .cs = {17},
	                  },
	    .sdCardDetect = {
	                  .detect = {22},
	                  .insertedLevel = GpioActiveLevel::Low,
	                  },
	    .psram = {
	                  .spi = {
	            .index = -1,
	            .sck = {21},
	            .mosi = {2},
	            .miso = {3},
	            .cs = {20},
	            .baudRate = LEXXOS_PICOCALC_PSRAM_SPI_BAUDRATE,
	        },
	                  .sio2 = {4},
	                  .sio3 = {5},
	                  },
	    .audioBeep = {
	                  .enabled = true,
	                  .left = {26},
	                  .right = {27},
	                  .defaultFrequencyHz = LEXXOS_PICOCALC_AUDIO_BEEP_FREQUENCY_HZ,
	                  .defaultDurationMs = LEXXOS_PICOCALC_AUDIO_BEEP_DURATION_MS,
	                  },
#if BOARD_ENABLED(PICOCALC_PICO_W)
	    .wireless = {
	                  .wifi = true,
	                  .bluetooth = true,
	                  .radio = "CYW43",
	                  .interfaceName = "wlan0",
	                  .backend = "pico-cyw43",
	                  .autoConnect = true,
	                  .scan = true,
	                  },
#else
	    .wireless = {},
#endif
	    .spiTftDisplay = {
	                  .enabled = true,
	                  .cs = {13},
	                  .dc = {14},
	                  .reset = {15},
	                  .width = 320,
	                  .height = 320,
	                  .columnOffset = 0,
	                  .rowOffset = 0,
	                  .invertColors = true,
	                  },
	};

	return descriptor;
}

} // namespace lexxos::platform
