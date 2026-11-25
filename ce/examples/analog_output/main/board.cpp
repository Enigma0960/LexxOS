#include "board.h"

#include <sdkconfig.h>

namespace analog_output {

namespace {

	constexpr lexxos::platform::AnalogOutputConfig outputs[]{
	    {.name = "reference", .pin = {CONFIG_DEMO_DAC_GPIO}, .resolutionBits = 8, .initialCode = 0},
	};

	lexxos::platform::LegacyBoardDescriptor const& board() {
		static constexpr lexxos::platform::LegacyBoardDescriptor descriptor{
		    .boardId = "esp_wroom_32",
		    .boardName = "ESP-WROOM-32 / Analog output",
		    .processorFamily = lexxos::platform::ProcessorFamily::Esp32,
		    .debugUart = {.index = 0, .tx = {1}, .rx = {3}, .baudRate = 115200},
		    .analogOutputs = outputs,
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

} // namespace analog_output
