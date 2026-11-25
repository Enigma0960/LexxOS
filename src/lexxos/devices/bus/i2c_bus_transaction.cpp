#include <lexxos/devices/bus/i2c_bus_transaction.h>

#include <lexxos/devices/bus/i2c_bus.h>

namespace lexxos::devices {

I2cBusTransaction::I2cBusTransaction(I2cBus& bus,
    DeviceResourcePriority priority,
    std::uint32_t timeoutMs)
    : _bus(&bus)
    , _active(bus._queue.acquire(priority, timeoutMs)) {
	if (_active && !bus.beginTransaction()) {
		bus._queue.release();
		_active = false;
	}
}

I2cBusTransaction::I2cBusTransaction(I2cBusTransaction&& other) noexcept
    : _bus(other._bus)
    , _active(other._active) {
	other._bus = nullptr;
	other._active = false;
}

I2cBusTransaction::~I2cBusTransaction() {
	release();
}

bool I2cBusTransaction::active() const {
	return _active;
}

I2cBusTransaction::operator bool() const {
	return active();
}

void I2cBusTransaction::release() {
	if (_bus != nullptr && _active) {
		_bus->endTransaction();
		_bus->_queue.release();
	}

	_bus = nullptr;
	_active = false;
}

} // namespace lexxos::devices

namespace lexxos::devices {

void I2cBus::setTransactionWaitFunction(I2cTransactionQueue::WaitFunction wait) {
	_queue.setWaitFunction(wait);
}

bool I2cBus::transactionBusy() const {
	return _queue.busy();
}

std::uint32_t I2cBus::pendingTransactions() const {
	return _queue.pending();
}

} // namespace lexxos::devices

namespace lexxos::devices {

I2cBusTransaction& I2cBusTransaction::operator=(I2cBusTransaction&& other) noexcept {
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
