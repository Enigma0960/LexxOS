#pragma once

#include <lexxos/devices/bus/bus_transaction_queue.h>

namespace lexxos::devices {

// Владение очередью для драйверов, которые не реализуют SpiBus.
class BusTransaction {
public:
	explicit BusTransaction(BusTransactionQueue& queue,
	    DeviceResourcePriority priority = DeviceResourcePriority::System, std::uint32_t timeoutMs = 100);
	~BusTransaction();
	BusTransaction(BusTransaction const&) = delete;
	BusTransaction& operator=(BusTransaction const&) = delete;

	[[nodiscard]] explicit operator bool() const { return _active; }

	[[nodiscard]] bool owns(BusTransactionQueue const& queue) const { return _active && &_queue == &queue; }

private:
	BusTransactionQueue& _queue;
	bool _active;
};

} // namespace lexxos::devices
