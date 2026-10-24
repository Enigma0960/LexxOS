#pragma once

#include <cstddef>
#include <cstdint>

#include <lexxos/platform/board.h>

namespace lexxos::devices {

class I2cBus {
public:
	virtual ~I2cBus() = default;

	virtual bool initialize(lexxos::platform::I2cPins const& pins) = 0;
	[[nodiscard]] virtual bool isReady() const = 0;
	virtual bool probe(std::uint8_t address) = 0;
	virtual bool write(std::uint8_t address, std::uint8_t const* data, std::size_t size) = 0;

	virtual bool read(std::uint8_t address, std::uint8_t* data, std::size_t size) {
		(void) address;
		(void) data;
		(void) size;
		return false;
	}

	virtual bool beginTransaction() {
		return true;
	}

	virtual void endTransaction() {
	}
};

class I2cBusTransaction {
public:
	explicit I2cBusTransaction(I2cBus& bus)
	    : _bus(&bus)
	    , _active(bus.beginTransaction()) {
	}

	I2cBusTransaction(I2cBusTransaction const&) = delete;
	I2cBusTransaction& operator=(I2cBusTransaction const&) = delete;

	I2cBusTransaction(I2cBusTransaction&& other) noexcept
	    : _bus(other._bus)
	    , _active(other._active) {
		other._bus = nullptr;
		other._active = false;
	}

	I2cBusTransaction& operator=(I2cBusTransaction&& other) noexcept {
		if (this != &other) {
			release();
			_bus = other._bus;
			_active = other._active;
			other._bus = nullptr;
			other._active = false;
		}
		return *this;
	}

	~I2cBusTransaction() {
		release();
	}

	[[nodiscard]] bool active() const {
		return _active;
	}

	[[nodiscard]] explicit operator bool() const {
		return active();
	}

	void release() {
		if (_bus != nullptr && _active) {
			_bus->endTransaction();
		}
		_bus = nullptr;
		_active = false;
	}

private:
	I2cBus* _bus{nullptr};
	bool _active{false};
};

I2cBus& debugI2cBus();
I2cBus& systemI2cBus();

} // namespace lexxos::devices
