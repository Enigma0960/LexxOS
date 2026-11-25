#pragma once
#include <memory>

#include <lexxos/devices/bus/i2c_bus.h>

namespace lexxos::platform::espidf {

std::unique_ptr<devices::I2cBus> createCeI2cBus();

} // namespace lexxos::platform::espidf
