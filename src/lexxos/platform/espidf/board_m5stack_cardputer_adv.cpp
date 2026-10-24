#include <lexxos/platform/board.h>

namespace lexxos::platform {

#ifndef LEXXOS_CARDPUTER_LCD_SPI_BAUDRATE
	#define LEXXOS_CARDPUTER_LCD_SPI_BAUDRATE 40000000
#endif

#ifndef LEXXOS_CARDPUTER_LCD_MADCTL
	#define LEXXOS_CARDPUTER_LCD_MADCTL 0x60
#endif

#ifndef LEXXOS_CARDPUTER_SD_SPI_BAUDRATE
	#define LEXXOS_CARDPUTER_SD_SPI_BAUDRATE 10000000
#endif

#ifdef LEXXOS_CARDPUTER_LCD_ADDRESS_WINDOW_SWAPS_AXES
	#define LEXXOS_CARDPUTER_LCD_SWAP_AXES true
#else
	#define LEXXOS_CARDPUTER_LCD_SWAP_AXES false
#endif

BoardDescriptor const& activeBoardDescriptor() {
	static BoardDescriptor const descriptor{
	    .boardId = "m5stack_cardputer_adv",
	    .boardName = "M5Stack Cardputer-Adv",
	    .vendor = "M5Stack",
	    .processorFamily = ProcessorFamily::Esp32,
	    .processorName = "ESP32-S3FN8 / Stamp-S3A",

	    .statusLed = {invalidPin},
	    .statusLedActiveLevel = {},
	    .userGpio = {
	                  .pins = {
	            {{1}, "hy2-g1", true, true, GpioPull::None, true},
	            {{2}, "hy2-g2", true, true, GpioPull::None, true},
	            {{3}, "ext-g3", true, true, GpioPull::None, true},
	            {{4}, "ext-g4", true, true, GpioPull::None, true},
	            {{5}, "ext-g5", true, true, GpioPull::None, true},
	            {{6}, "ext-g6", true, true, GpioPull::None, true},
	            {{13}, "ext-uart-rx-g13", true, false, GpioPull::None, false},
	            {{15}, "ext-uart-tx-g15", true, false, GpioPull::None, false},
	            {{39}, "ext-sd-miso-g39", true, false, GpioPull::None, false},
	            {{40}, "ext-sd-sck-g40", true, false, GpioPull::None, false},
	            {{14}, "ext-sd-mosi-g14", true, false, GpioPull::None, false},
	            {{8}, "ext-i2c-sda-g8", true, false, GpioPull::Up, false},
	            {{9}, "ext-i2c-scl-g9", true, false, GpioPull::Up, false},
	        },
	                  .count = 13,
	                  },

	    // Cardputer-Adv routes the expansion UART to EXT pins G15/G13.
	    // USB Serial/JTAG remains enabled by the ESP-IDF console backend as a
	    // second diagnostics channel.
	    .debugUart = {
	                  .index = 0,
	                  .tx = {15},
	                  .rx = {13},
	                  .baudRate = 115200,
	                  },

	    .interfaces = {
	                  .consoleUart = {.index = 0, .tx = {15}, .rx = {13}, .baudRate = 115200},
	                  // Cardputer-Adv system I2C bus: TCA8418 keyboard controller,
	        // BMI270 IMU and ES8311 audio codec share G8/G9.
	        .systemI2c = {
	            .index = 0,
	            .sda = {8},
	            .scl = {9},
	            .baudRate = 400000,
	        },
	                  .storageSpi = {
	            .index = 0,
	            .sck = {40},
	            .mosi = {14},
	            .miso = {39},
	            .baudRate = LEXXOS_CARDPUTER_SD_SPI_BAUDRATE,
	        },
	                  .displaySpi = {
	            .index = 1,
	            .sck = {36},
	            .mosi = {35},
	            .baudRate = LEXXOS_CARDPUTER_LCD_SPI_BAUDRATE,
	        },
	                  },

	    .sdInterface = SdInterface::Spi,
	    .sdSpi = {
	                  .index = 0,
	                  .sck = {40},
	                  .mosi = {14},
	                  .miso = {39},
	                  .cs = {12},
	                  .baudRate = LEXXOS_CARDPUTER_SD_SPI_BAUDRATE,
	                  },
	    .sdSdio = {},
	    .sdCardDetect = {},
	    .psram = {},
	    .audioCodec = {
	                  .enabled = true,
	                  .name = "cardputer-adv-audio",
	                  .microphoneName = "cardputer-adv-mic",
	                  .codec = AudioCodecKind::Es8311,
	                  .i2sIndex = 1,
	                  .mclk = {invalidPin},
	                  .bclk = {41},
	                  .wordSelect = {43},
	                  .dataOut = {42},
	                  .dataIn = {46},
	                  .i2cAddress = 0x18,
	                  .sampleRateHz = 44100,
	                  .defaultFrequencyHz = 880,
	                  .defaultDurationMs = 120,
	                  },
	    .motionSensor = {
	                  .enabled = true,
	                  .sensor = MotionSensorKind::Bmi270,
	                  .name = "cardputer-bmi270",
	                  .i2cAddress = 0x69,
	                  },
	    .infraredTransmitter = {
	                  .enabled = true,
	                  .name = "cardputer-ir",
	                  .output = {44},
	                  .carrierFrequencyHz = 38000,
	                  },
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
