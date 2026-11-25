#include <lexxos/devices/bus/software_i2c_bus.h>

namespace lexxos::devices {

bool SoftwareI2cBus::initialize(lexxos::platform::I2cPins const& pins) {
	_ready = false;
	if (!pins.isValid() || pins.sda.number == pins.scl.number || pins.baudRate < 1000 || pins.baudRate > 100000 || !_io.configure || !_io.release || !_io.read || !_io.counter || !_io.frequency) {
		return false;
	}

	auto const frequency = _io.frequency(_io.context);
	if (frequency == 0) {
		return false;
	}

	_pins = pins;
	auto const divisor = 2U * pins.baudRate;
	_halfPeriod = (static_cast<std::uint64_t>(frequency) + divisor - 1U) / divisor;
	_timeout = (static_cast<std::uint64_t>(frequency) + 49U) / 50U;
	_ready = _io.configure(_io.context, pins);
	return _ready;
}

bool SoftwareI2cBus::clearBus() {
	if (!_ready) {
		return false;
	}

	_begin();
	_io.release(_io.context, _pins.sda, true);
	bool ok = _raiseClock();
	for (unsigned pulse = 0; ok && pulse < 9 && !_io.read(_io.context, _pins.sda); ++pulse) {
		_io.release(_io.context, _pins.scl, false);
		_delay();
		ok = _raiseClock();
	}

	return _finish(ok);
}

bool SoftwareI2cBus::probe(std::uint8_t address) {
	if (!_ready || address > 0x7FU) {
		return false;
	}

	_begin();
	bool const ok = _start() && _writeByte(static_cast<std::uint8_t>(address << 1U));
	return _finish(ok);
}

bool SoftwareI2cBus::write(std::uint8_t address, std::uint8_t const* data, std::size_t size) {
	if (!_ready || address > 0x7FU || data == nullptr || size == 0) {
		return false;
	}

	_begin();

	bool ok = _start() && _writeByte(static_cast<std::uint8_t>(address << 1U));
	for (std::size_t i = 0; ok && i < size; ++i) {
		ok = _writeByte(data[i]);
	}

	return _finish(ok);
}

bool SoftwareI2cBus::read(std::uint8_t address, std::uint8_t* data, std::size_t size) {
	if (!_ready || address > 0x7FU || data == nullptr || size == 0) {
		return false;
	}

	_begin();

	bool ok = _start() && _writeByte(static_cast<std::uint8_t>((address << 1U) | 1U));
	for (std::size_t i = 0; ok && i < size; ++i) {
		ok = _readByte(data[i], i + 1U == size);
	}

	return _finish(ok);
}

void SoftwareI2cBus::_begin() {
	_started = _io.counter(_io.context);
}

bool SoftwareI2cBus::_expired() const {
	return _io.counter(_io.context) - _started >= _timeout;
}

void SoftwareI2cBus::_delay() const {
	auto const start = _io.counter(_io.context);
	while (_io.counter(_io.context) - start < _halfPeriod) {
	}
}

bool SoftwareI2cBus::_raiseClock() const {
	_io.release(_io.context, _pins.scl, true);
	while (!_io.read(_io.context, _pins.scl)) {
		if (_expired()) {
			return false;
		}
	}

	_delay();
	return !_expired();
}

bool SoftwareI2cBus::_stop() const {
	_io.release(_io.context, _pins.scl, false);
	_io.release(_io.context, _pins.sda, false);
	_delay();

	bool const clockOk = _raiseClock();
	_io.release(_io.context, _pins.sda, true);
	_delay();

	return clockOk && _io.read(_io.context, _pins.sda);
}

bool SoftwareI2cBus::_finish(bool ok) const {
	bool const stopped = _stop();
	// Release both pins even if clock stretching exceeded the deadline.
	_io.release(_io.context, _pins.sda, true);
	_io.release(_io.context, _pins.scl, true);
	return ok && stopped;
}

bool SoftwareI2cBus::_start() const {
	_io.release(_io.context, _pins.sda, true);
	if (!_raiseClock()) {
		return false;
	}

	// Recover an interrupted MCU byte (e.g. after a CPU-only SWD reload).
	if (!_io.read(_io.context, _pins.sda)) {
		for (unsigned i = 0; i < 9 && !_io.read(_io.context, _pins.sda); ++i) {
			_io.release(_io.context, _pins.scl, false);
			_delay();

			if (!_raiseClock()) {
				return false;
			}
		}

		if (!_stop()) {
			return false;
		}
	}

	_io.release(_io.context, _pins.sda, false);
	_delay();
	_io.release(_io.context, _pins.scl, false);

	return true;
}

bool SoftwareI2cBus::_writeBit(bool high) const {
	_io.release(_io.context, _pins.sda, high);
	_delay();
	bool const ok = _raiseClock();
	_io.release(_io.context, _pins.scl, false);
	return ok;
}

bool SoftwareI2cBus::_readBit(bool& high) const {
	_io.release(_io.context, _pins.sda, true);
	_delay();
	bool const ok = _raiseClock();
	high = _io.read(_io.context, _pins.sda);
	_io.release(_io.context, _pins.scl, false);
	return ok;
}

bool SoftwareI2cBus::_writeByte(std::uint8_t value) const {
	for (unsigned bit = 0; bit < 8; ++bit) {
		if (!_writeBit((value & (0x80U >> bit)) != 0)) {
			return false;
		}
	}

	bool nack = true;
	return _readBit(nack) && !nack;
}

bool SoftwareI2cBus::_readByte(std::uint8_t& value, bool last) const {
	value = 0;
	for (unsigned bit = 0; bit < 8; ++bit) {
		bool high = false;
		if (!_readBit(high)) {
			return false;
		}

		value = static_cast<std::uint8_t>((value << 1U) | (high ? 1U : 0U));
	}

	return _writeBit(last); // ACK intermediate bytes, NACK the last byte.
}

} // namespace lexxos::devices
