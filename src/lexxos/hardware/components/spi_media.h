#pragma once

#include <lexxos/hardware/components/spi_endpoint.h>

namespace lexxos::hardware::components {

struct SpiSdCard {
	char const* name;
	SpiEndpoint endpoint;
	modules::rp2040::Pin cardDetect;
	bool detectLowMeansInserted;
};

struct QuadSpiPsram {
	SpiEndpoint endpoint;
	modules::rp2040::Pin sio2;
	modules::rp2040::Pin sio3;
};

} // namespace lexxos::hardware::components
