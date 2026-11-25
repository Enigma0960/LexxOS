#pragma once
#include <lexxos/devices/bus/spi_controller.h>

namespace lexxos::platform::espidf {

std::unique_ptr<devices::SpiController> createCeSpiController();

} // namespace lexxos::platform::espidf
