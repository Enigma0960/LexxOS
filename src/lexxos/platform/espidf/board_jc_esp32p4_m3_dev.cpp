#include <lexxos/platform/board.h>

namespace lexxos::platform {

BoardDescriptor const& activeBoardDescriptor() {
	static BoardDescriptor const descriptor{
	    .boardId = "jc_esp32p4_m3_dev",
	    .boardName = "Guition JC-ESP32P4-M3-DEV",
	    .vendor = "Guition / Jingcai",
	    .processorFamily = ProcessorFamily::Esp32,
	    .processorName = "ESP32-P4NRW32 / ESP32-C6-MINI-1U-N4",

	    // Power LED is not routed as a user-controlled status LED.
	    .statusLed = {invalidPin},
	    .statusLedActiveLevel = {},

	    // JP1 26-pin expansion header. These are the 11 user GPIOs called out by
	    // the board documentation; several of them are shared with optional
	    // connectors, so keep new device drivers conservative until each net is
	    // smoke-tested on the real board.
	    .userGpio = {
	                  .pins = {
	            {{1}, "jp1-gpio1", true, true, GpioPull::None, true},
	            {{2}, "jp1-gpio2", true, true, GpioPull::None, true},
	            {{3}, "jp1-gpio3", true, true, GpioPull::None, true},
	            {{4}, "jp1-gpio4-spi2-clk", true, true, GpioPull::None, true},
	            {{5}, "jp1-gpio5", true, true, GpioPull::None, true},
	            {{20}, "jp1-gpio20-dsi-shared", true, true, GpioPull::None, true},
	            {{32}, "jp1-gpio32", true, true, GpioPull::None, true},
	            {{33}, "jp1-gpio33", true, true, GpioPull::None, true},
	            {{45}, "jp1-gpio45", true, true, GpioPull::None, true},
	            {{46}, "jp1-gpio46", true, true, GpioPull::None, true},
	            {{47}, "jp1-gpio47", true, true, GpioPull::None, true},
	        },
	                  .count = 11,
	                  },

	    // USB-TTL connector, CH340C on board. This is the minimal bring-up
	    // terminal path used by /dev/tty.usbserial-2130 at 115200 baud.
	    .debugUart = {
	                  .index = 0,
	                  .tx = {37},
	                  .rx = {38},
	                  .baudRate = 115200,
	                  },

	    .interfaces = {
	                  .consoleUart = {.index = 0, .tx = {37}, .rx = {38}, .baudRate = 115200},
	                  // Shared board I2C: FocalTech touch, ES8311 codec control,
	        // MIPI-DSI backlight controller and camera connector sideband.
	        .systemI2c = {
	            .index = 0,
	            .sda = {7},
	            .scl = {8},
	            .baudRate = 400000,
	        },
	                  },

	    // Native SDMMC/SDIO TF card slot.
	    .sdInterface = SdInterface::Sdio4Bit,
	    .sdSpi = {},
	    .sdSdio = {
	                  .index = 0,
	                  .clock = {43},
	                  .command = {44},
	                  .data0 = {39},
	                  .data1 = {40},
	                  .data2 = {41},
	                  .data3 = {42},
	                  .initializationClockHz = 400000,
	                  .transferClockHz = 20000000,
	                  .powerLdoChannel = 4,
	                  },
	    .sdCardDetect = {},

	    // ESP32-P4NRW32 has 32 MB in-package PSRAM. It is configured through
	    // ESP-IDF sdkconfig, not through GPIO pins exposed to BoardDescriptor.
	    .psram = {},
	    .audioCodec = {
	                  .enabled = true,
	                  .name = "jc-esp32p4-es8311-audio",
	                  .microphoneName = "jc-esp32p4-es8311-mic",
	                  .codec = AudioCodecKind::Es8311,
	                  .i2sIndex = 0,
	                  .mclk = {13},
	                  .bclk = {12},
	                  .wordSelect = {10},
	                  .dataOut = {9},
	                  .dataIn = {48},
	                  .amplifierEnable = {11},
	                  .amplifierEnableLevel = GpioActiveLevel::High,
	                  .i2cAddress = 0x18,
	                  .sampleRateHz = 48000,
	                  .defaultFrequencyHz = 880,
	                  .defaultDurationMs = 250,
	                  },

	    .wireless = {
	                  .wifi = true,
	                  .bluetooth = false,
	                  .radio = "ESP32-C6 SDIO coprocessor",
	                  .interfaceName = "wifi0",
	                  .backend = "esp-hosted-sdio",
	                  .autoConnect = true,
	                  .scan = true,
	                  .hostedSdio = {
	            .index = 1,
	            .clock = {18},
	            .command = {19},
	            .data0 = {14},
	            .data1 = {15},
	            .data2 = {16},
	            .data3 = {17},
	            .initializationClockHz = 400000,
	            .transferClockHz = 40000000,
	        },
	                  .hostedReset = {54},
	                  },

	    .ethernet = {
	                  .enabled = true,
	                  .phy = EthernetPhyKind::Ip101,
	                  .name = "eth0",
	                  .mdc = {31},
	                  .mdio = {52},
	                  .reset = {51},
	                  .clock = {50},
	                  .txd0 = {34},
	                  .txd1 = {35},
	                  .txEnable = {49},
	                  .rxd0 = {29},
	                  .rxd1 = {30},
	                  .crsDv = {28},
	                  .phyAddress = 1,
	                  },

	    // No discrete battery-backed RTC IC is populated on this board. LexxOS
	    // provides an uptime-backed software RTC and keeps it refreshed from NTP
	    // whenever Ethernet or Wi-Fi has a DHCP lease.
	    .softwareRtc = {
	                  .enabled = true,
	                  .preferAsDefault = true,
	                  .autoNetworkSync = true,
	                  .networkSyncServer = "pool.ntp.org",
	                  .networkSyncTimeoutMs = 8000,
	                  .networkSyncIntervalMs = 60U * 60U * 1000U,
	                  .networkSyncRetryIntervalMs = 60U * 1000U,
	                  .startupDelayMs = 3000,
	                  },

	    .mipiDsiDisplay = {
	                  .enabled = true,
	                  .name = "btt-tft70-dsi",
	                  .panelKind = MipiDsiPanelKind::RaspberryPiTouch7InchV1,
	                  .width = 800,
	                  .height = 480,
	                  .virtualChannel = 0,
	                  .dataLanes = 1,
	                  .laneBitRateMbps = 600,
	                  .dpiClockKhz = 25979,
	                  .inputColor = MipiDsiColorFormat::Rgb888,
	                  .outputColor = MipiDsiColorFormat::Rgb888,
	                  .rgbElementOrder = MipiDsiRgbElementOrder::Bgr,
	                  .frameBuffers = 1,
	                  .hsyncPulseWidth = 2,
	                  .hsyncBackPorch = 46,
	                  .hsyncFrontPorch = 210,
	                  .vsyncPulseWidth = 20,
	                  .vsyncBackPorch = 4,
	                  .vsyncFrontPorch = 22,
	                  .dsiPhyPowerLdoChannel = 3,
	                  .dsiPhyPowerMillivolts = 2500,
	                  .powerControlEnabled = true,
	                  .powerControlRequired = false,
	                  .powerControlI2cIndex = 0,
	                  .powerControlAddress = 0x45,
	                  .backlightPwmRegister = 0x86,
	                  .defaultBrightness = 255,
	                  .tc358762BridgeInit = true,
	                  .nonBurstSyncPulses = true,
	                  .forceClockLaneHs = true,
	                  .disableLowPower = false,
	                  .frameAck = false,
	                  .commandAck = false,
	                  },

	    .touchPanel = {
	                  .enabled = true,
	                  .controller = TouchPanelKind::FocalTechFt5x06,
	                  .name = "ft5x06-touch",
	                  .i2cIndex = 0,
	                  .address = 0x38,
	                  .interrupt = {invalidPin},
	                  .reset = {invalidPin},
	                  .interruptActiveLevel = GpioActiveLevel::Low,
	                  .rawWidth = 800,
	                  .rawHeight = 480,
	                  .swapXY = false,
	                  .invertX = false,
	                  .invertY = false,
	                  },
	};

	/*
	 * JC-ESP32P4-M3-DEV interface map for follow-up device support.
	 *
	 * Core memory:
	 *   Flash: 16 MB NOR, QIO
	 *   PSRAM: 32 MB in-package HEX PSRAM, ESP-IDF SPIRAM at 200 MHz
	 *
	 * Console/debug:
	 *   USB-TTL CH340C: UART0 TX GPIO37, RX GPIO38, BOOTMODE via DTR, CHIP_PU via RTS
	 *   Native USB Serial/JTAG: ESP32-P4 USB_SERIAL_JTAG, also initialized by the ESP-IDF console backend
	 *   BOOT button: BOOTMODE strap
	 *   EN button: CHIP_PU reset
	 *
	 * Expansion header JP1:
	 *   1  3V3           2  5V
	 *   3  GND           4  GND
	 *   5  GPIO1         6  NC/reserved
	 *   7  GPIO2         8  GPIO47
	 *   9  GPIO3         10 GPIO46
	 *   11 GPIO4         12 GPIO45
	 *   13 GPIO5         14 NC/reserved
	 *   15 GPIO20        16 GND
	 *   17 GPIO32        18 3V3
	 *   19 GPIO33        20 ESP32-C6 U0RXD
	 *   21 ES_I2C_SDA    22 ESP32-C6 U0TXD
	 *   23 ES_I2C_SCL    24 ESP32-C6 IO9
	 *   25 NC/reserved   26 ESP32-C6 CHIP_PU
	 *
	 * ESP32-C6 hosted Wi-Fi/Bluetooth over SDIO:
	 *   reset/chip-pu GPIO54
	 *   SDIO CMD GPIO19, CLK GPIO18
	 *   SDIO D0 GPIO14, D1 GPIO15, D2 GPIO16, D3 GPIO17
	 *   C6 sideband on JP1: U0RXD, U0TXD, IO9, CHIP_PU
	 *
	 * Ethernet IP101:
	 *   RMII TXD1 GPIO35, TXD0 GPIO34, TXEN GPIO49
	 *   RMII RXD1 GPIO30, RXD0 GPIO29, RXDV GPIO28
	 *   MDC GPIO31, MDIO GPIO52, PHY_RSTN/PWR GPIO51
	 *   RMII CLK GPIO50, PHY address 1
	 *
	 * System I2C:
	 *   SDA GPIO7, SCL GPIO8, 400 kHz
	 *   ES8311 codec address 0x18
	 *   FocalTech/FT5x06-compatible touch controller on DSI FPC
	 *   MIPI-DSI backlight controller uses I2C PWM register 0x96
	 *   RTC note: vendor schematic labels RTC_DAT/SDA1 and RTC_CLK/SCL1
	 *   on the touch FPC sideband, but no dedicated RTC IC is confirmed on
	 *   the public schematic set. The board profile enables LexxOS software
	 *   RTC as the default backend and can still probe PCF8563/BM8563-compatible
	 *   RTCs on this bus with --source pcf8563, default address 0x51.
	 *
	 * Audio ES8311 / speaker / microphone:
	 *   I2S0 MCLK GPIO13, BCLK/SCLK GPIO12, LRCK/WS GPIO10
	 *   I2S0 DOUT/DSDIN GPIO9
	 *   ES7210/ADC SDOUT GPIO48
	 *   PA/SPK enable GPIO11, active high
	 *   Speaker header: 8 ohm / 2 W
	 *   Onboard analog microphone feeds the codec ADC path.
	 *
	 * TF/microSD slot:
	 *   SD_CLK GPIO43, SD_CMD GPIO44
	 *   SD_DATA0 GPIO39, DATA1 GPIO40, DATA2 GPIO41, DATA3 GPIO42
	 *   SD power LDO channel 4 is configured by ESP-IDF examples for 2.7 V.
	 *
	 * MIPI DSI 2-lane display FPC:
	 *   DSI_A_DATA1_P/N GPIO20/GPIO21
	 *   DSI_A_CLK_P/N   GPIO22/GPIO23
	 *   DSI_A_DATA0_P/N GPIO24/GPIO25
	 *   Touch sideband: ES_I2C_SDA GPIO7, ES_I2C_SCL GPIO8
	 *   LCD_RST and LCD_LED are not direct GPIOs on the vendor examples.
	 *   Current enabled display profile:
	 *     BIGTREETECH Pi TFT70 V2.1 / Raspberry Pi DSI-compatible 7" panel
	 *     800x480, LexxOS RGB565 API -> BGR888 framebuffer -> RGB888 video
	 *     DSI lane0 + clock, 600 Mbps/lane, 25.979 MHz DPI pixel clock
	 *     timings: HFP=210 HSW=2 HBP=46, VFP=22 VSW=20 VBP=4
	 *     DSI PHY power: internal LDO channel 3 at 2.5 V
	 *     display control MCU/backlight: I2C 0x45, PWM register 0x86
	 *     FocalTech/FT5x06-compatible touch controller is on the same I2C
	 *     sideband, observed at address 0x38, raw/default geometry 800x480.
	 *
	 * MIPI CSI 2-lane camera FPC:
	 *   CSI_A_DATA0_P/N GPIO34/GPIO33
	 *   CSI_A_DATA1_P/N GPIO36/GPIO35
	 *   CSI_A_CLK_P/N   GPIO38/GPIO37
	 *   CSI_IO0 GPIO26, CSI_IO1 GPIO27
	 *   Sideband: ES_I2C_SDA GPIO7, ES_I2C_SCL GPIO8
	 *
	 * USB:
	 *   USB 2.0 high-speed OTG: ESP_USB_P/N
	 *   USB 1.1 full-speed OTG: USB1P1_P/N
	 *   USB-TTL CH340C debug/programming port: USB-DP/DM to UART0
	 *
	 * RS485:
	 *   TX1 GPIO26, RX1 GPIO27, EN/DE tied in the transceiver circuit.
	 *
	 * Battery:
	 *   Charger/power path: IP5306
	 *   Battery ADC divider: GPIO53 in vendor battery example path
	 */

	return descriptor;
}

} // namespace lexxos::platform
