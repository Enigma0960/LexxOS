#pragma once

#include <lexxos/devices/bus/software_i2c_bus.h>
#include <lexxos/devices/rk3506/rk3506_i2c_bus.h>

namespace lexxos::platform::rk3506 {

// Factories provide callbacks only; no controller instance or MMIO at creation.
// Both adapters implement the existing GPIO0_B3/B2 route.
devices::Rk3506I2cIo hardwareI2c2Io();
devices::SoftwareI2cIo softwareI2c2Io();

} // namespace lexxos::platform::rk3506
