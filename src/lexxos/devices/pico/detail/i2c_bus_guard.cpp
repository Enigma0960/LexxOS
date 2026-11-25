#include <lexxos/devices/pico/detail/i2c_bus_guard.h>

namespace lexxos::devices {

I2cBusGuard::I2cBusGuard(I2cBusMutex& mutex)
    : _mutex(mutex)
    , _locked(_mutex.lock()) {
}

I2cBusGuard::~I2cBusGuard() {
	if (_locked) {
		_mutex.unlock();
	}
}

bool I2cBusGuard::locked() const {
	return _locked;
}

} // namespace lexxos::devices
