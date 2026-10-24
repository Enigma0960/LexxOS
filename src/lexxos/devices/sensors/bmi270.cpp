#include <lexxos/devices/sensors/bmi270.h>

#include <cstddef>

#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/sensors/bmi270_config.h>
#include <lexxos/platform/platform.h>

namespace lexxos::devices {
namespace {

	constexpr std::uint8_t bmi270ChipId = 0x24;
	constexpr std::uint8_t bmi270SoftResetCommand = 0xB6;
	constexpr std::uint8_t registerChipId = 0x00;
	constexpr std::uint8_t registerError = 0x02;
	constexpr std::uint8_t registerStatus = 0x03;
	constexpr std::uint8_t registerGyroData = 0x0C;
	constexpr std::uint8_t registerInternalStatus = 0x21;
	constexpr std::uint8_t registerTemperature = 0x22;
	constexpr std::uint8_t registerAccelConfig = 0x40;
	constexpr std::uint8_t registerAccelRange = 0x41;
	constexpr std::uint8_t registerGyroConfig = 0x42;
	constexpr std::uint8_t registerGyroRange = 0x43;
	constexpr std::uint8_t registerInitControl = 0x59;
	constexpr std::uint8_t registerInitAddress0 = 0x5B;
	constexpr std::uint8_t registerInitData = 0x5E;
	constexpr std::uint8_t registerPowerConfig = 0x7C;
	constexpr std::uint8_t registerPowerControl = 0x7D;
	constexpr std::uint8_t registerCommand = 0x7E;
	constexpr std::uint8_t configLoadStatusMask = 0x0F;
	constexpr std::uint8_t configLoadSuccess = 0x01;
	constexpr std::size_t configUploadChunkBytes = 32;

	std::int16_t readLeSigned16(std::uint8_t const* data) {
		return static_cast<std::int16_t>(static_cast<std::uint16_t>(data[0]) | (static_cast<std::uint16_t>(data[1]) << 8U));
	}

} // namespace

Bmi270Sensor::Bmi270Sensor(
    I2cBus& bus,
    lexxos::platform::MotionSensorPins config,
    lexxos::platform::PlatformServices const& platform)
    : _bus(bus)
    , _config(config)
    , _platform(&platform) {
}

bool Bmi270Sensor::initialize() {
	_state = SensorState::Unavailable;
	_lastSample = {};

	if (!_config.isValid() || !_bus.isReady() || !_bus.probe(_config.i2cAddress)) {
		return false;
	}

	std::uint8_t chipId = 0;
	if (!_readRegister(registerChipId, chipId) || chipId != bmi270ChipId) {
		_lastSample.chipId = chipId;
		_state = SensorState::Fault;
		return false;
	}

	if (!_softReset() || !_readRegister(registerChipId, chipId) || chipId != bmi270ChipId) {
		_lastSample.chipId = chipId;
		_state = SensorState::Fault;
		return false;
	}

	_lastSample.chipId = chipId;
	if (!_writeConfigFile() || !_configureBasicSampling()) {
		_state = SensorState::Fault;
		return false;
	}
	_state = SensorState::Ready;
	return poll();
}

std::string_view Bmi270Sensor::name() const {
	return _config.name != nullptr ? std::string_view{_config.name} : std::string_view{"bmi270"};
}

SensorState Bmi270Sensor::state() const {
	return _state;
}

bool Bmi270Sensor::poll() {
	if (_state != SensorState::Ready) {
		return false;
	}

	MotionSensorSample sample{};
	sample.chipId = _lastSample.chipId;
	sample.timestampMs = _uptimeMs();
	(void) _readRegister(registerStatus, sample.status);
	(void) _readRegister(registerError, sample.error);

	std::uint8_t data[12]{};
	if (_readBlock(registerGyroData, data, sizeof(data))) {
		sample.gyroX = readLeSigned16(&data[0]);
		sample.gyroY = readLeSigned16(&data[2]);
		sample.gyroZ = readLeSigned16(&data[4]);
		sample.accelX = readLeSigned16(&data[6]);
		sample.accelY = readLeSigned16(&data[8]);
		sample.accelZ = readLeSigned16(&data[10]);
	}

	std::uint8_t temperature[2]{};
	if (_readBlock(registerTemperature, temperature, sizeof(temperature))) {
		auto const raw = readLeSigned16(temperature);
		sample.temperatureCentiC = static_cast<std::int16_t>(2300 + (static_cast<int>(raw) * 100) / 512);
	}
	sample.valid = true;
	_lastSample = sample;
	return true;
}

MotionSensorSample Bmi270Sensor::lastSample() const {
	return _lastSample;
}

bool Bmi270Sensor::readSample(MotionSensorSample& sample) {
	if (!poll()) {
		return false;
	}
	sample = _lastSample;
	return true;
}

bool Bmi270Sensor::_softReset() {
	if (!_writeRegister(registerCommand, bmi270SoftResetCommand)) {
		return false;
	}
	_sleepMs(2);
	return true;
}

bool Bmi270Sensor::_writeConfigFile() {
	if (!_writeRegister(registerPowerConfig, 0x00)) {
		return false;
	}
	_sleepMs(1);
	if (!_writeRegister(registerInitControl, 0x00)) {
		return false;
	}

	for (std::uint16_t index = 0; index < bmi270_config::size; index = static_cast<std::uint16_t>(index + configUploadChunkBytes)) {
		auto const remaining = bmi270_config::size - index;
		auto const chunk = remaining < configUploadChunkBytes ? remaining : configUploadChunkBytes;
		if (!_uploadConfigChunk(index, &bmi270_config::data[index], chunk)) {
			return false;
		}
	}

	if (!_writeRegister(registerInitControl, 0x01)) {
		return false;
	}
	_sleepMs(20);

	std::uint8_t internalStatus = 0;
	if (!_readInternalStatus(internalStatus)) {
		return false;
	}
	_lastSample.status = internalStatus;
	return (internalStatus & configLoadStatusMask) == configLoadSuccess;
}

bool Bmi270Sensor::_uploadConfigChunk(std::uint16_t index, std::uint8_t const* data, std::size_t size) {
	if (data == nullptr || size == 0 || size > configUploadChunkBytes || (size % 2U) != 0) {
		return false;
	}
	auto const wordIndex = static_cast<std::uint16_t>(index / 2U);
	std::uint8_t const address[] = {
	    static_cast<std::uint8_t>(wordIndex & 0x0FU),
	    static_cast<std::uint8_t>(wordIndex >> 4U),
	};
	return _writeBlock(registerInitAddress0, address, sizeof(address)) && _writeBlock(registerInitData, data, size);
}

bool Bmi270Sensor::_readInternalStatus(std::uint8_t& status) {
	return _readRegister(registerInternalStatus, status);
}

bool Bmi270Sensor::_readRegister(std::uint8_t reg, std::uint8_t& value) {
	return _readBlock(reg, &value, 1);
}

bool Bmi270Sensor::_readBlock(std::uint8_t reg, std::uint8_t* data, std::size_t size) {
	if (data == nullptr || size == 0) {
		return false;
	}
	I2cBusTransaction transaction{_bus};
	return transaction && _bus.write(_config.i2cAddress, &reg, 1) && _bus.read(_config.i2cAddress, data, size);
}

bool Bmi270Sensor::_writeRegister(std::uint8_t reg, std::uint8_t value) {
	return _writeBlock(reg, &value, 1);
}

bool Bmi270Sensor::_writeBlock(std::uint8_t reg, std::uint8_t const* data, std::size_t size) {
	if (data == nullptr || size == 0 || size > configUploadChunkBytes) {
		return false;
	}
	std::uint8_t payload[configUploadChunkBytes + 1]{};
	payload[0] = reg;
	for (std::size_t index = 0; index < size; ++index) {
		payload[index + 1] = data[index];
	}
	I2cBusTransaction transaction{_bus};
	return transaction && _bus.write(_config.i2cAddress, payload, size + 1);
}

bool Bmi270Sensor::_configureBasicSampling() {
	if (!_writeRegister(registerPowerConfig, 0x00)) {
		return false;
	}
	_sleepMs(2);
	if (!_writeRegister(registerAccelConfig, 0xA8)
	    || !_writeRegister(registerAccelRange, 0x03)
	    || !_writeRegister(registerGyroConfig, 0xA9)
	    || !_writeRegister(registerGyroRange, 0x00)
	    || !_writeRegister(registerPowerControl, 0x0E)) {
		return false;
	}
	_sleepMs(50);
	return true;
}

std::uint32_t Bmi270Sensor::_uptimeMs() const {
	return _platform != nullptr && _platform->clock.uptimeMs != nullptr ? _platform->clock.uptimeMs() : lexxos::platform::uptimeMs();
}

void Bmi270Sensor::_sleepMs(std::uint32_t milliseconds) const {
	if (_platform != nullptr && _platform->clock.sleepMs != nullptr) {
		_platform->clock.sleepMs(milliseconds);
		return;
	}
	lexxos::platform::sleepMs(milliseconds);
}

} // namespace lexxos::devices
