#pragma once

#include <lexxos/devices/pico/detail/spi_bus_mutex.h>

namespace lexxos::devices {

class SpiBusGuard {
public:
	explicit SpiBusGuard(SpiBusMutex& mutex);

	~SpiBusGuard();

	[[nodiscard]] bool locked() const;

private:
	SpiBusMutex& _mutex;
	bool _locked{false};
};

} // namespace lexxos::devices
