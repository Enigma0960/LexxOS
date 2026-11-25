#pragma once

namespace lexxos::devices {

class I2cBus;

} // namespace lexxos::devices

namespace lexxos::composition::products {

// Single boot-lifetime owner. Construction does not initialize hardware.
devices::I2cBus& lyraSouthbridgeBus();

} // namespace lexxos::composition::products
