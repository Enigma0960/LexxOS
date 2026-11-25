#include "board.h"

#include <sdkconfig.h>

namespace spi_devices {

namespace {

	constexpr lexxos::platform::SpiDeviceConfig peripherals[]{
	    {.name = "first", .cs = {CONFIG_DEMO_SPI_CS_FIRST}, .baudRate = 1000000},
	    {.name = "second", .cs = {CONFIG_DEMO_SPI_CS_SECOND}, .baudRate = 2000000, .mode = lexxos::devices::SpiClockMode::Mode3},
	};
	constexpr lexxos::platform::NamedSpiBus buses[]{
	    {.name = "peripherals",
	     .pins = {.index = 0, .sck = {CONFIG_DEMO_SPI_SCK}, .mosi = {CONFIG_DEMO_SPI_MOSI}, .miso = {CONFIG_DEMO_SPI_MISO}},
	     .devices = peripherals},
	};

	lexxos::platform::LegacyBoardDescriptor const& board() {
		static constexpr lexxos::platform::LegacyBoardDescriptor descriptor{
		    .boardId = "esp_wroom_32",
		    .boardName = "ESP-WROOM-32 / SPI devices",
		    .processorFamily = lexxos::platform::ProcessorFamily::Esp32,
		    .debugUart = {.index = 0, .tx = {1}, .rx = {3}, .baudRate = 115200},
		    .spiBuses = buses,
		};
		return descriptor;
	}

} // namespace

lexxos::platform::PlatformServices const& platform() {
	static auto const services = [] {
		auto value = lexxos::platform::services();
		value.board.current = board;
		return value;
	}();
	return services;
}

} // namespace spi_devices
