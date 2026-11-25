#pragma once

#include <lexxos/hardware/components/spi_endpoint.h>

namespace lexxos::hardware::components {

struct SpiTft {
	SpiEndpoint endpoint;
	modules::rp2040::Pin dataCommand;
	modules::rp2040::Pin reset;
	modules::rp2040::Pin backlight;
	int width;
	int height;
	bool invertColors;
};

} // namespace lexxos::hardware::components
