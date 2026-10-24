#include <lexxos/devices/rtc/pcf8563.h>

#include <array>
#include <cstddef>

#include <lexxos/devices/bus/i2c_bus.h>

namespace lexxos::devices::rtc {

namespace {

	constexpr std::uint8_t secondsRegister = 0x02;
	constexpr std::uint8_t voltageLowMask = 0x80;
	constexpr std::uint8_t monthCenturyMask = 0x80;

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

} // namespace

Pcf8563Rtc::Pcf8563Rtc(I2cBus& bus, std::uint8_t address)
    : _bus(&bus)
    , _address(address) {
}

bool Pcf8563Rtc::probe() {
	return _bus != nullptr && _bus->probe(_address);
}

bool Pcf8563Rtc::readDateTime(DateTime& output) {
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
	if (!fromBcd(static_cast<std::uint8_t>(registers[2] & 0x3FU), 23U, value.hour)) {
		return false;
	}
	if (!fromBcd(static_cast<std::uint8_t>(registers[3] & 0x3FU), 31U, value.day) || value.day == 0U) {
		return false;
	}
	if (!fromBcd(static_cast<std::uint8_t>(registers[5] & 0x1FU), 12U, value.month) || value.month == 0U) {
		return false;
	}

	std::uint8_t year = 0;
	if (!fromBcd(registers[6], 99U, year)) {
		return false;
	}
	value.year = static_cast<std::uint16_t>((registers[5] & monthCenturyMask) != 0U ? 2100U + year : 2000U + year);
	if (!isValidDate(value)) {
		return false;
	}

	value.weekday = weekdayIso(value);
	output = value;
	return true;
}

bool Pcf8563Rtc::writeDateTime(DateTime const& value) {
	if (!isValidDate(value)) {
		return false;
	}

	std::uint16_t const relativeYear = value.year >= 2100U
	    ? static_cast<std::uint16_t>(value.year - 2100U)
	    : static_cast<std::uint16_t>(value.year - 2000U);
	if (relativeYear > 99U) {
		return false;
	}

	std::uint8_t const isoWeekday = value.weekday == 0U ? weekdayIso(value) : value.weekday;
	std::array<std::uint8_t, 7> registers{
	    toBcd(value.second),
	    toBcd(value.minute),
	    toBcd(value.hour),
	    toBcd(value.day),
	    toBcd(static_cast<std::uint8_t>(isoWeekday % 7U)),
	    static_cast<std::uint8_t>(toBcd(value.month) | (value.year >= 2100U ? monthCenturyMask : 0x00U)),
	    toBcd(static_cast<std::uint8_t>(relativeYear)),
	};

	return writeRegisters(secondsRegister, registers.data(), static_cast<std::uint8_t>(registers.size()));
}

bool Pcf8563Rtc::readStatus(Pcf8563Status& output) {
	std::uint8_t seconds = 0;
	if (!readRegisters(secondsRegister, &seconds, 1)) {
		return false;
	}

	output = Pcf8563Status{
	    .voltageLow = (seconds & voltageLowMask) != 0U,
	    .secondsRaw = seconds,
	};
	return true;
}

bool Pcf8563Rtc::readRegisters(std::uint8_t firstRegister, std::uint8_t* data, std::uint8_t size) {
	if (_bus == nullptr || data == nullptr || size == 0U) {
		return false;
	}

	lexxos::devices::I2cBusTransaction transaction{*_bus};
	if (!transaction) {
		return false;
	}

	return _bus->write(_address, &firstRegister, 1) && _bus->read(_address, data, size);
}

bool Pcf8563Rtc::writeRegisters(std::uint8_t firstRegister, std::uint8_t const* data, std::uint8_t size) {
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
