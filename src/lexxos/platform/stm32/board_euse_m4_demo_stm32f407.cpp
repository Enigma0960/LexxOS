#include <lexxos/platform/board.h>

namespace lexxos::platform {
namespace {

	constexpr int stm32PinNumber(char port, int pin) {
		return (static_cast<int>(port) - static_cast<int>('A')) * 16 + pin;
	}

	constexpr GpioPin stm32Pin(char port, int pin) {
		return GpioPin{stm32PinNumber(port, pin)};
	}

} // namespace

BoardDescriptor const& activeBoardDescriptor() {
	static BoardDescriptor const descriptor{
	    .boardId = "euse_m4_demo_stm32f407",
	    .boardName = "Euse M4 DEMO Large on STM32F407ZGT6",
	    .vendor = "Euse",
	    .processorFamily = ProcessorFamily::Stm32f4,
	    .processorName = "STM32F407ZGT6",
	    .statusLed = stm32Pin('C', 0),
	    .statusLedActiveLevel = GpioActiveLevel::Low,
	    .debugUart = {
	                  .index = 1,
	                  .tx = stm32Pin('A', 9),
	                  .rx = stm32Pin('A', 10),
	                  .baudRate = 115200,
	                  },
	    .debugOledI2c = {},
	    .sdInterface = SdInterface::Sdio1Bit,
	    .sdSpi = {},
	    .sdSdio = {
	                  .index = 1,
	                  .clock = stm32Pin('C', 12),
	                  .command = stm32Pin('D', 2),
	                  .data0 = stm32Pin('C', 8),
	                  .data1 = {},
	                  .data2 = {},
	                  .data3 = stm32Pin('C', 11),
	                  .initializationClockHz = 400000,
	                  .transferClockHz = 24000000,
	                  },
	    .sdCardDetect = {},
	    .psram = {},
	    .spiTftDisplay = {},
	    .parallelTftDisplay = {
	                  .enabled = true,
	                  .name = "eu-tft3.2",
	                  .width = 320,
	                  .height = 240,
	                  .chipSelect = stm32Pin('G', 12),
	                  .registerSelect = stm32Pin('G', 0),
	                  .read = stm32Pin('D', 5),
	                  .write = stm32Pin('D', 4),
	                  .reset = stm32Pin('C', 5),
	                  .backlight = stm32Pin('B', 0),
	                  .data = {
	            stm32Pin('D', 14),
	            stm32Pin('D', 15),
	            stm32Pin('D', 0),
	            stm32Pin('D', 1),
	            stm32Pin('E', 7),
	            stm32Pin('E', 8),
	            stm32Pin('E', 9),
	            stm32Pin('E', 10),
	            stm32Pin('E', 11),
	            stm32Pin('E', 12),
	            stm32Pin('E', 13),
	            stm32Pin('E', 14),
	            stm32Pin('E', 15),
	            stm32Pin('D', 8),
	            stm32Pin('D', 9),
	            stm32Pin('D', 10),
	        },
	                  },
	};

	return descriptor;
}

} // namespace lexxos::platform
