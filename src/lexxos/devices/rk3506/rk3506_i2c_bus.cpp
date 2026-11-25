#include <lexxos/devices/rk3506/rk3506_i2c_bus.h>

#include <algorithm>

namespace lexxos::devices {

namespace {

	constexpr std::uint32_t con = 0x00, clkdiv = 0x04, mrxaddr = 0x08, mrxraddr = 0x0C;
	constexpr std::uint32_t mtxcnt = 0x10, mrxcnt = 0x14, ien = 0x18, ipd = 0x1C;
	constexpr std::uint32_t txdata = 0x100, rxdata = 0x200;
	constexpr std::uint32_t enable = 1U, trx = 2U, rx = 4U, start = 8U, stop = 16U, lastNack = 32U;
	constexpr std::uint32_t txDone = 4U, rxDone = 8U, startDone = 16U, stopDone = 32U, nack = 64U;
	constexpr std::uint32_t pendingMask = 0x1FFU;
	// RK3506 TRM, RKI2C_CON: SDA setup/hold configuration and standard-mode START.
	constexpr std::uint32_t timing = (1U << 8U) | (1U << 12U);
	constexpr std::size_t fifoBytes = 32;

} // namespace

bool Rk3506I2cBus::initialize(lexxos::platform::I2cPins const& pins) {
	_ready = false;
	_needsRecovery = false;
	_recoveryCoolingDown = false;
	if ((pins.baudRate != 10000U && pins.baudRate != 100000U)
	    || !_io.configure
	    || !_io.read
	    || !_io.write
	    || !_io.counter
	    || !_io.frequency) {
		return false;
	}

	auto const frequency = _io.frequency(_io.context);
	if (!frequency || !_io.configure(_io.context, pins)) {
		return false;
	}

	_timeout = (static_cast<std::uint64_t>(frequency) + 49U) / 50U;
	_recoveryBackoff = (static_cast<std::uint64_t>(frequency) + 3U) / 4U;
	_write(con, 0);
	_write(ien, 0); // IPD is polled; the platform masks this controller at the GIC.
	_write(ipd, pendingMask);
	// OSC24M / 1: each half-period is 8 * (divider + 1) cycles.
	// 100 kHz -> divider 14; 10 kHz -> divider 149.
	auto const divider = 1500000U / pins.baudRate - 1U;
	_clockDivider = (divider << 16U) | divider;
	_pins = pins;
	_write(clkdiv, _clockDivider);
	_ready = _read(clkdiv) == _clockDivider;
	return _ready;
}

std::uint32_t Rk3506I2cBus::_read(std::uint32_t offset) const {
	return _io.read(_io.context, offset);
}

void Rk3506I2cBus::_write(std::uint32_t offset, std::uint32_t value) const {
	_io.write(_io.context, offset, value);
}

bool Rk3506I2cBus::_begin() {
	if (_needsRecovery && _io.recover) {
		auto const now = _io.counter(_io.context);
		if (_recoveryCoolingDown && now - _lastRecoveryAttempt < _recoveryBackoff) {
			return false;
		}

		_lastRecoveryAttempt = now;
		_recoveryCoolingDown = true;
		++_recoveryAttempts;
		if (!_io.recover(_io.context, _pins)) {
			return false;
		}

		_write(clkdiv, _clockDivider);
		if (_read(clkdiv) != _clockDivider) {
			return false;
		}

		++_recoverySuccesses;
		_recoveryCoolingDown = false;
	}

	_needsRecovery = false;
	_lastFailure = {};
	_write(con, 0);
	_write(ipd, pendingMask);
	return true;
}

bool Rk3506I2cBus::_wait(std::uint32_t event, bool checkNack) const {
	auto const started = _io.counter(_io.context);
	bool expired = false;
	std::uint32_t pending{};
	for (;;) {
		pending = _read(ipd);

		// A NACK can arrive together with completion. It must win.
		if (checkNack && (pending & nack)) {
			if (!_lastFailure.available) {
				_lastFailure = {true, true, event, pending};
			}

			return false;
		}

		if (pending & event) {
			_write(ipd, event);
			return true;
		}

		// Completion may have arrived while a higher-priority task ran. Inspect
		// IPD before the wall-clock deadline; each hardware phase has its own budget.
		if (expired) {
			break;
		}

		expired = _io.counter(_io.context) - started >= _timeout;
	}

	if (!_lastFailure.available) {
		_lastFailure = {true, false, event, pending};
	}

	return false;
}

bool Rk3506I2cBus::_start(std::uint32_t control) const {
	_write(ien, startDone | nack);
	_write(con, control | timing | enable | start);
	if (!_wait(startDone)) {
		return false;
	}

	_write(con, control | timing | enable);
	return true;
}

bool Rk3506I2cBus::_finish(bool ok) {
	// STOP gets a fresh bounded deadline, even after a data timeout/NACK.
	_write(ipd, pendingMask);
	_write(ien, stopDone);
	_write(con, enable | timing | stop);

	bool const stopped = _wait(stopDone, false);
	_write(con, 0);
	_write(ien, 0);
	_write(ipd, pendingMask);

	_needsRecovery = !stopped || (_lastFailure.available && !_lastFailure.nack);
	return ok && stopped;
}

bool Rk3506I2cBus::probe(std::uint8_t address) {
	return _ready && address <= 0x7FU && _transmit(address, nullptr, 0);
}

bool Rk3506I2cBus::write(std::uint8_t address, std::uint8_t const* data, std::size_t size) {
	if (!_ready || address > 0x7FU || !data || !size || size == static_cast<std::size_t>(-1)) {
		return false;
	}
	return _transmit(address, data, size);
}

bool Rk3506I2cBus::_transmit(std::uint8_t address, std::uint8_t const* data, std::size_t size) {
	if (!_begin()) {
		return false;
	}

	std::size_t offset = 0;
	while (offset < size + 1U) {
		auto const count = std::min(fifoBytes, size + 1U - offset);
		for (std::size_t word = 0; word < (count + 3U) / 4U; ++word) {
			std::uint32_t packed = 0;
			for (std::size_t byte = 0; byte < 4 && word * 4 + byte < count; ++byte) {
				auto const index = offset + word * 4 + byte;
				auto const value = index == 0 ? static_cast<std::uint8_t>(address << 1U) : data[index - 1U];
				packed |= static_cast<std::uint32_t>(value) << (byte * 8U);
			}

			_write(txdata + static_cast<std::uint32_t>(word * 4U), packed);
		}

		if (offset == 0 && !_start(0)) {
			return _finish(false);
		}

		_write(ien, txDone | nack);
		_write(mtxcnt, static_cast<std::uint32_t>(count));
		if (!_wait(txDone)) {
			return _finish(false);
		}

		offset += count;
	}

	return _finish(true);
}

bool Rk3506I2cBus::read(std::uint8_t address, std::uint8_t* data, std::size_t size) {
	if (!_ready || address > 0x7FU || !data || !size) {
		return false;
	}

	if (!_begin()) {
		return false;
	}

	_write(mrxaddr, (1U << 24U) | (address << 1U) | 1U);
	_write(mrxraddr, 0); // Raw read; STM32 register selection was a separate write + STOP.
	std::size_t offset = 0;
	while (offset < size) {
		auto const count = std::min(fifoBytes, size - offset);
		auto const control = (offset == 0 ? trx : rx) | (offset + count == size ? lastNack : 0U);
		if (offset == 0) {
			if (!_start(control)) {
				return _finish(false);
			}
		} else {
			_write(con, enable | timing | control);
		}

		_write(ien, rxDone | nack);
		_write(mrxcnt, static_cast<std::uint32_t>(count));
		if (!_wait(rxDone)) {
			return _finish(false);
		}

		for (std::size_t word = 0; word < (count + 3U) / 4U; ++word) {
			auto const packed = _read(rxdata + static_cast<std::uint32_t>(word * 4U));
			for (std::size_t byte = 0; byte < 4 && word * 4 + byte < count; ++byte) {
				data[offset + word * 4 + byte] = static_cast<std::uint8_t>(packed >> (byte * 8U));
			}
		}

		offset += count;
	}

	return _finish(true);
}

} // namespace lexxos::devices
