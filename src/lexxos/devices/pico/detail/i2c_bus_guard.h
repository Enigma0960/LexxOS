#pragma once

#include <lexxos/devices/pico/detail/i2c_bus_mutex.h>

namespace lexxos::devices {

class I2cBusGuard {
public:
	explicit I2cBusGuard(I2cBusMutex& mutex);

	~I2cBusGuard();

	[[nodiscard]] bool locked() const;

private:
	I2cBusMutex& _mutex;
	bool _locked{false};
};

} // namespace lexxos::devices
