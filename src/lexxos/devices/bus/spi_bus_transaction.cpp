#include <lexxos/devices/bus/spi_bus_transaction.h>

#include <lexxos/devices/bus/spi_bus.h>

namespace lexxos::devices {

SpiBusTransaction::SpiBusTransaction(SpiBus& bus,
    DeviceResourcePriority priority,
    std::uint32_t timeoutMs)
    : _bus(&bus)
    , _active(bus._queue->acquire(priority, timeoutMs)) {
	if (_active && !bus.beginTransaction()) {
		bus._queue->release();
		_active = false;
	}
}

SpiBusTransaction::SpiBusTransaction(SpiBusTransaction&& other) noexcept
    : _bus(other._bus)
    , _active(other._active) {
	other._bus = nullptr;
	other._active = false;
}

SpiBusTransaction::~SpiBusTransaction() {
	release();
}

bool SpiBusTransaction::active() const {
	return _active;
}

SpiBusTransaction::operator bool() const {
	return active();
}

void SpiBusTransaction::release() {
	if (_bus != nullptr && _active) {
		_bus->endTransaction();
		_bus->_queue->release();
	}

	_bus = nullptr;
	_active = false;
}

} // namespace lexxos::devices

namespace lexxos::devices {

void SpiBus::setTransactionWaitFunction(BusTransactionQueue::WaitFunction wait) {
	_queue->setWaitFunction(wait);
}

bool SpiBus::transactionBusy() const {
	return _queue->busy();
}

std::uint32_t SpiBus::pendingTransactions() const {
	return _queue->pending();
}

} // namespace lexxos::devices

namespace lexxos::devices {

SpiBusTransaction& SpiBusTransaction::operator=(SpiBusTransaction&& other) noexcept {
	if (this != &other) {
		release();
		_bus = other._bus;
		_active = other._active;
		other._bus = nullptr;
		other._active = false;
	}

	return *this;
}

} // namespace lexxos::devices
