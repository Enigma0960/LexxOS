#pragma once

#include <lexxos/hardware/modules/rp2040.h>

namespace lexxos::hardware::components {

struct SpiEndpoint {
	modules::rp2040::SpiId controller;
	modules::rp2040::Pin chipSelect;
};

} // namespace lexxos::hardware::components
