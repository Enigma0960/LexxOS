#include "board.h"

#include <sdkconfig.h>

namespace i2c_sensor {

namespace {

	static constexpr lexxos::platform::NamedI2cBus buses[]{
	    {"sensors", {.index = 0, .sda = {CONFIG_DEMO_I2C_SDA}, .scl = {CONFIG_DEMO_I2C_SCL}, .baudRate = 100000}},
	};

	lexxos::platform::LegacyBoardDescriptor const& board() {
		static constexpr lexxos::platform::LegacyBoardDescriptor descriptor{
		    .boardId = "esp_wroom_32",
		    .boardName = "ESP-WROOM-32 / I2C sensors",
		    .processorFamily = lexxos::platform::ProcessorFamily::Esp32,
		    .debugUart = {.index = 0, .tx = {1}, .rx = {3}, .baudRate = 115200},
		    .i2cBuses = buses,
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

} // namespace i2c_sensor
