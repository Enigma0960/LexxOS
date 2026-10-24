#include <lexxos/devices/rtc/ds3231.h>

#include <array>
#include <cstddef>

#include <lexxos/devices/bus/i2c_bus.h>

namespace lexxos::devices::rtc {

namespace {

	constexpr std::uint8_t secondsRegister = 0x00;
	constexpr std::uint8_t statusRegister = 0x0F;
	constexpr std::uint8_t oscillatorStoppedMask = 0x80;

	std::uint8_t toBcd(std::uint8_t value) {
		return static_cast<std::uint8_t>(((value / 10U) << 4U) | (value % 10U));
	}

	bool fromBcd(std::uint8_t value, std::uint8_t max, std::uint8_t& output) {
		std::uint8_t const low = static_cast<std::uint8_t>(value & 0x0FU);
		std::uint8_t const high = static_cast<std::uint8_t>((value >> 4U) & 0x0FU);
		if (low > 9U || high > 9U) {
			return false;
		}

		std::uint8_t const decoded = static_cast<std::uint8_t>(high * 10U + low);
		if (decoded > max) {
			return false;
		}

		output = decoded;
		return true;
	}

	bool read24Hour(std::uint8_t value, std::uint8_t& hour) {
		if ((value & 0x40U) == 0U) {
			return fromBcd(static_cast<std::uint8_t>(value & 0x3FU), 23U, hour);
		}

		std::uint8_t decoded = 0;
		if (!fromBcd(static_cast<std::uint8_t>(value & 0x1FU), 12U, decoded) || decoded == 0U) {
			return false;
		}

		bool const pm = (value & 0x20U) != 0U;
		if (decoded == 12U) {
			hour = pm ? 12U : 0U;
		} else {
			hour = static_cast<std::uint8_t>(pm ? decoded + 12U : decoded);
		}
		return hour <= 23U;
	}

} // namespace

Ds3231Rtc::Ds3231Rtc(I2cBus& bus, std::uint8_t address)
    : _bus(&bus)
    , _address(address) {
}

bool Ds3231Rtc::probe() {
	return _bus != nullptr && _bus->probe(_address);
}

bool Ds3231Rtc::readDateTime(DateTime& output) {
	std::array<std::uint8_t, 7> registers{};
	if (!readRegisters(secondsRegister, registers.data(), static_cast<std::uint8_t>(registers.size()))) {
		return false;
	}

	DateTime value{};
	if (!fromBcd(static_cast<std::uint8_t>(registers[0] & 0x7FU), 59U, value.second)) {
		return false;
	}
	if (!fromBcd(static_cast<std::uint8_t>(registers[1] & 0x7FU), 59U, value.minute)) {
		return false;
	}
	if (!read24Hour(registers[2], value.hour)) {
		return false;
	}
	if (!fromBcd(static_cast<std::uint8_t>(registers[3] & 0x07U), 7U, value.weekday) || value.weekday == 0U) {
		return false;
	}
	if (!fromBcd(static_cast<std::uint8_t>(registers[4] & 0x3FU), 31U, value.day) || value.day == 0U) {
		return false;
	}
	if (!fromBcd(static_cast<std::uint8_t>(registers[5] & 0x1FU), 12U, value.month) || value.month == 0U) {
		return false;
	}

	std::uint8_t year = 0;
	if (!fromBcd(registers[6], 99U, year)) {
		return false;
	}
	value.year = static_cast<std::uint16_t>((registers[5] & 0x80U) != 0U ? 2100U + year : 2000U + year);
	if (!isValidDate(value)) {
		return false;
	}

	output = value;
	return true;
}

bool Ds3231Rtc::writeDateTime(DateTime const& value) {
	if (!isValidDate(value)) {
		return false;
	}

	std::uint16_t const relativeYear = value.year >= 2100U
	    ? static_cast<std::uint16_t>(value.year - 2100U)
	    : static_cast<std::uint16_t>(value.year - 2000U);
	if (relativeYear > 99U) {
		return false;
	}

	std::array<std::uint8_t, 7> registers{
	    toBcd(value.second),
	    toBcd(value.minute),
	    toBcd(value.hour),
	    toBcd(value.weekday == 0U ? weekdayIso(value) : value.weekday),
	    toBcd(value.day),
	    static_cast<std::uint8_t>(toBcd(value.month) | (value.year >= 2100U ? 0x80U : 0x00U)),
	    toBcd(static_cast<std::uint8_t>(relativeYear)),
	};

	return writeRegisters(secondsRegister, registers.data(), static_cast<std::uint8_t>(registers.size()))
	    && clearOscillatorStoppedFlag();
}

bool Ds3231Rtc::readStatus(Ds3231Status& output) {
	std::uint8_t status = 0;
	if (!readRegisters(statusRegister, &status, 1)) {
		return false;
	}

	output = Ds3231Status{
	    .oscillatorStopped = (status & oscillatorStoppedMask) != 0U,
	    .raw = status,
	};
	return true;
}

bool Ds3231Rtc::clearOscillatorStoppedFlag() {
	Ds3231Status status{};
	if (!readStatus(status)) {
		return false;
	}

	std::uint8_t const cleared = static_cast<std::uint8_t>(status.raw & ~oscillatorStoppedMask);
	return writeRegisters(statusRegister, &cleared, 1);
}

bool Ds3231Rtc::readRegisters(std::uint8_t firstRegister, std::uint8_t* data, std::uint8_t size) {
	if (_bus == nullptr || data == nullptr || size == 0U) {
		return false;
	}

	lexxos::devices::I2cBusTransaction transaction{*_bus};
	if (!transaction) {
		return false;
	}

	return _bus->write(_address, &firstRegister, 1) && _bus->read(_address, data, size);
}

bool Ds3231Rtc::writeRegisters(std::uint8_t firstRegister, std::uint8_t const* data, std::uint8_t size) {
	if (_bus == nullptr || data == nullptr || size == 0U) {
		return false;
	}

	std::array<std::uint8_t, 8> buffer{};
	if (size + 1U > buffer.size()) {
		return false;
	}

	buffer[0] = firstRegister;
	for (std::uint8_t index = 0; index < size; ++index) {
		buffer[static_cast<std::size_t>(index) + 1U] = data[index];
	}

	return _bus->write(_address, buffer.data(), static_cast<std::size_t>(size + 1U));
}

} // namespace lexxos::devices::rtc
