#include <lexxos/devices/bus/bus_transaction.h>

namespace lexxos::devices {

BusTransaction::BusTransaction(BusTransactionQueue& queue, DeviceResourcePriority priority, std::uint32_t timeoutMs)
    : _queue(queue)
    , _active(queue.acquire(priority, timeoutMs)) {
}

BusTransaction::~BusTransaction() {
	if (_active) {
		_queue.release();
	}
}

} // namespace lexxos::devices
