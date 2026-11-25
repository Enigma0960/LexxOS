#include <lexxos/devices/pico/detail/spi_bus_guard.h>

namespace lexxos::devices {

SpiBusGuard::SpiBusGuard(SpiBusMutex& mutex)
    : _mutex(mutex)
    , _locked(_mutex.lock()) {
}

SpiBusGuard::~SpiBusGuard() {
	if (_locked) {
		_mutex.unlock();
	}
}

bool SpiBusGuard::locked() const {
	return _locked;
}

} // namespace lexxos::devices
