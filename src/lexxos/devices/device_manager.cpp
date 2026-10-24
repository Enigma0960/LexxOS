#include <lexxos/devices/device_manager.h>

#include <utility>

namespace lexxos::devices {

DeviceResourceLock::DeviceResourceLock(DeviceManager& manager, Kind kind, std::size_t index)
    : _manager(&manager)
    , _kind(kind)
    , _index(index) {
}

DeviceResourceLock::DeviceResourceLock(DeviceResourceLock&& other) noexcept
    : _manager(std::exchange(other._manager, nullptr))
    , _kind(std::exchange(other._kind, Kind::None))
    , _index(std::exchange(other._index, 0)) {
}

DeviceResourceLock& DeviceResourceLock::operator=(DeviceResourceLock&& other) noexcept {
	if (this != &other) {
		release();
		_manager = std::exchange(other._manager, nullptr);
		_kind = std::exchange(other._kind, Kind::None);
		_index = std::exchange(other._index, 0);
	}
	return *this;
}

DeviceResourceLock::~DeviceResourceLock() {
	release();
}

bool DeviceResourceLock::valid() const {
	return _manager != nullptr && _kind != Kind::None;
}

DeviceResourceLock::operator bool() const {
	return valid();
}

void DeviceResourceLock::release() {
	if (_manager == nullptr || _kind == Kind::None) {
		return;
	}

	_manager->_release(_kind, _index);
	_manager = nullptr;
	_kind = Kind::None;
	_index = 0;
}

void DeviceManager::initialize() {
	_i2cBuses.clear();
	_blockDevices.clear();
	_displayDevices.clear();
	_inputDevices.clear();
	_pointingDevices.clear();
	_gamepadDevices.clear();
	_sensorDevices.clear();
	_infraredDevices.clear();
	_picoCalcSouthbridgeServices.clear();
}

void DeviceManager::initialize(lexxos::platform::PlatformServices const&) {
	initialize();
}

bool DeviceManager::registerI2cBus(std::string_view name, std::string_view description, I2cBus& bus) {
	return _i2cBuses.add({
	    .name = name,
	    .description = description,
	    .bus = &bus,
	});
}

bool DeviceManager::registerBlockDevice(std::string_view name, std::string_view description, BlockDevice& device) {
	return _blockDevices.add({
	    .name = name,
	    .description = description,
	    .device = &device,
	});
}

bool DeviceManager::registerDisplayDevice(std::string_view name, std::string_view description, DisplayDevice& device) {
	return _displayDevices.add({
	    .name = name,
	    .description = description,
	    .device = &device,
	});
}

bool DeviceManager::registerInputDevice(std::string_view name, std::string_view description, InputDevice& device) {
	return _inputDevices.add({
	    .name = name,
	    .description = description,
	    .device = &device,
	});
}

bool DeviceManager::registerPointingDevice(std::string_view name, std::string_view description, PointingDevice& device) {
	return _pointingDevices.add({
	    .name = name,
	    .description = description,
	    .device = &device,
	});
}

bool DeviceManager::registerGamepadDevice(std::string_view name, std::string_view description, GamepadDevice& device) {
	return _gamepadDevices.add({
	    .name = name,
	    .description = description,
	    .device = &device,
	});
}

bool DeviceManager::registerSensorDevice(std::string_view name, std::string_view description, Sensor& device) {
	return _sensorDevices.add({
	    .name = name,
	    .description = description,
	    .device = &device,
	});
}

bool DeviceManager::registerInfraredDevice(std::string_view name, std::string_view description, InfraredTransmitter& device) {
	return _infraredDevices.add({
	    .name = name,
	    .description = description,
	    .device = &device,
	});
}

bool DeviceManager::registerPicoCalcSouthbridgeService(
    std::string_view name,
    std::string_view description,
    PicoCalcSouthbridgeRuntimeService& service) {
	return _picoCalcSouthbridgeServices.add({
	    .name = name,
	    .description = description,
	    .service = &service,
	});
}

I2cBus* DeviceManager::i2cBus(std::string_view name) {
	auto* descriptor = _i2cBuses.find(name);
	return descriptor != nullptr ? descriptor->bus : nullptr;
}

I2cBus const* DeviceManager::i2cBus(std::string_view name) const {
	auto const* descriptor = _i2cBuses.find(name);
	return descriptor != nullptr ? descriptor->bus : nullptr;
}

std::size_t DeviceManager::i2cBusCount() const {
	return _i2cBuses.count();
}

I2cBusDescriptor const& DeviceManager::i2cBusAt(std::size_t index) const {
	return _i2cBuses.at(index);
}

bool DeviceManager::isI2cBusLocked(std::string_view name) const {
	return _i2cBuses.isLocked(name);
}

bool DeviceManager::isI2cBusLockedAt(std::size_t index) const {
	return _i2cBuses.isLockedAt(index);
}

DeviceResourceStatus DeviceManager::i2cBusStatusAt(std::size_t index) const {
	return _i2cBuses.statusAt(index);
}

DeviceResourceLock DeviceManager::lockI2cBus(std::string_view name, DeviceResourcePriority priority, std::string_view owner) {
	auto const index = _i2cBuses.findIndex(name);
	if (index == detail::invalidDeviceResourceIndex || !_i2cBuses.tryLock(index, priority, owner)) {
		return {};
	}
	return DeviceResourceLock{*this, DeviceResourceLock::Kind::I2cBus, index};
}

BlockDevice* DeviceManager::blockDevice(std::string_view name) {
	auto* descriptor = _blockDevices.find(name);
	return descriptor != nullptr ? descriptor->device : nullptr;
}

BlockDevice const* DeviceManager::blockDevice(std::string_view name) const {
	auto const* descriptor = _blockDevices.find(name);
	return descriptor != nullptr ? descriptor->device : nullptr;
}

std::size_t DeviceManager::blockDeviceCount() const {
	return _blockDevices.count();
}

BlockDeviceDescriptor const& DeviceManager::blockDeviceAt(std::size_t index) const {
	return _blockDevices.at(index);
}

bool DeviceManager::isBlockDeviceLocked(std::string_view name) const {
	return _blockDevices.isLocked(name);
}

bool DeviceManager::isBlockDeviceLockedAt(std::size_t index) const {
	return _blockDevices.isLockedAt(index);
}

DeviceResourceStatus DeviceManager::blockDeviceStatusAt(std::size_t index) const {
	return _blockDevices.statusAt(index);
}

DeviceResourceLock DeviceManager::lockBlockDevice(std::string_view name, DeviceResourcePriority priority, std::string_view owner) {
	auto const index = _blockDevices.findIndex(name);
	if (index == detail::invalidDeviceResourceIndex || !_blockDevices.tryLock(index, priority, owner)) {
		return {};
	}
	return DeviceResourceLock{*this, DeviceResourceLock::Kind::BlockDevice, index};
}

DisplayDevice* DeviceManager::displayDevice(std::string_view name) {
	auto* descriptor = _displayDevices.find(name);
	return descriptor != nullptr ? descriptor->device : nullptr;
}

DisplayDevice const* DeviceManager::displayDevice(std::string_view name) const {
	auto const* descriptor = _displayDevices.find(name);
	return descriptor != nullptr ? descriptor->device : nullptr;
}

DisplayDevice* DeviceManager::primaryDisplayDevice() {
	return _displayDevices.count() > 0 ? _displayDevices.at(0).device : nullptr;
}

DisplayDevice const* DeviceManager::primaryDisplayDevice() const {
	return _displayDevices.count() > 0 ? _displayDevices.at(0).device : nullptr;
}

std::size_t DeviceManager::displayDeviceCount() const {
	return _displayDevices.count();
}

DisplayDeviceDescriptor const& DeviceManager::displayDeviceAt(std::size_t index) const {
	return _displayDevices.at(index);
}

bool DeviceManager::isDisplayDeviceLocked(std::string_view name) const {
	return _displayDevices.isLocked(name);
}

bool DeviceManager::isDisplayDeviceLockedAt(std::size_t index) const {
	return _displayDevices.isLockedAt(index);
}

DeviceResourceStatus DeviceManager::displayDeviceStatusAt(std::size_t index) const {
	return _displayDevices.statusAt(index);
}

DeviceResourceLock DeviceManager::lockDisplayDevice(
    std::string_view name,
    DeviceResourcePriority priority,
    std::string_view owner) {
	auto const index = _displayDevices.findIndex(name);
	if (index == detail::invalidDeviceResourceIndex || !_displayDevices.tryLock(index, priority, owner)) {
		return {};
	}
	return DeviceResourceLock{*this, DeviceResourceLock::Kind::DisplayDevice, index};
}

InputDevice* DeviceManager::inputDevice(std::string_view name) {
	auto* descriptor = _inputDevices.find(name);
	return descriptor != nullptr ? descriptor->device : nullptr;
}

InputDevice const* DeviceManager::inputDevice(std::string_view name) const {
	auto const* descriptor = _inputDevices.find(name);
	return descriptor != nullptr ? descriptor->device : nullptr;
}

InputDevice* DeviceManager::primaryInputDevice() {
	return _inputDevices.count() > 0 ? _inputDevices.at(0).device : nullptr;
}

InputDevice const* DeviceManager::primaryInputDevice() const {
	return _inputDevices.count() > 0 ? _inputDevices.at(0).device : nullptr;
}

std::size_t DeviceManager::inputDeviceCount() const {
	return _inputDevices.count();
}

InputDeviceDescriptor const& DeviceManager::inputDeviceAt(std::size_t index) const {
	return _inputDevices.at(index);
}

bool DeviceManager::isInputDeviceLocked(std::string_view name) const {
	return _inputDevices.isLocked(name);
}

bool DeviceManager::isInputDeviceLockedAt(std::size_t index) const {
	return _inputDevices.isLockedAt(index);
}

DeviceResourceStatus DeviceManager::inputDeviceStatusAt(std::size_t index) const {
	return _inputDevices.statusAt(index);
}

DeviceResourceLock DeviceManager::lockInputDevice(
    std::string_view name,
    DeviceResourcePriority priority,
    std::string_view owner) {
	auto const index = _inputDevices.findIndex(name);
	if (index == detail::invalidDeviceResourceIndex || !_inputDevices.tryLock(index, priority, owner)) {
		return {};
	}
	return DeviceResourceLock{*this, DeviceResourceLock::Kind::InputDevice, index};
}

PointingDevice* DeviceManager::pointingDevice(std::string_view name) {
	auto* descriptor = _pointingDevices.find(name);
	return descriptor != nullptr ? descriptor->device : nullptr;
}

PointingDevice const* DeviceManager::pointingDevice(std::string_view name) const {
	auto const* descriptor = _pointingDevices.find(name);
	return descriptor != nullptr ? descriptor->device : nullptr;
}

PointingDevice* DeviceManager::primaryPointingDevice() {
	return _pointingDevices.count() > 0 ? _pointingDevices.at(0).device : nullptr;
}

PointingDevice const* DeviceManager::primaryPointingDevice() const {
	return _pointingDevices.count() > 0 ? _pointingDevices.at(0).device : nullptr;
}

std::size_t DeviceManager::pointingDeviceCount() const {
	return _pointingDevices.count();
}

PointingDeviceDescriptor const& DeviceManager::pointingDeviceAt(std::size_t index) const {
	return _pointingDevices.at(index);
}

bool DeviceManager::isPointingDeviceLocked(std::string_view name) const {
	return _pointingDevices.isLocked(name);
}

bool DeviceManager::isPointingDeviceLockedAt(std::size_t index) const {
	return _pointingDevices.isLockedAt(index);
}

DeviceResourceStatus DeviceManager::pointingDeviceStatusAt(std::size_t index) const {
	return _pointingDevices.statusAt(index);
}

DeviceResourceLock DeviceManager::lockPointingDevice(
    std::string_view name,
    DeviceResourcePriority priority,
    std::string_view owner) {
	auto const index = _pointingDevices.findIndex(name);
	if (index == detail::invalidDeviceResourceIndex || !_pointingDevices.tryLock(index, priority, owner)) {
		return {};
	}
	return DeviceResourceLock{*this, DeviceResourceLock::Kind::PointingDevice, index};
}

GamepadDevice* DeviceManager::gamepadDevice(std::string_view name) {
	auto* descriptor = _gamepadDevices.find(name);
	return descriptor != nullptr ? descriptor->device : nullptr;
}

GamepadDevice const* DeviceManager::gamepadDevice(std::string_view name) const {
	auto const* descriptor = _gamepadDevices.find(name);
	return descriptor != nullptr ? descriptor->device : nullptr;
}

GamepadDevice* DeviceManager::primaryGamepadDevice() {
	return _gamepadDevices.count() > 0 ? _gamepadDevices.at(0).device : nullptr;
}

GamepadDevice const* DeviceManager::primaryGamepadDevice() const {
	return _gamepadDevices.count() > 0 ? _gamepadDevices.at(0).device : nullptr;
}

std::size_t DeviceManager::gamepadDeviceCount() const {
	return _gamepadDevices.count();
}

GamepadDeviceDescriptor const& DeviceManager::gamepadDeviceAt(std::size_t index) const {
	return _gamepadDevices.at(index);
}

bool DeviceManager::isGamepadDeviceLocked(std::string_view name) const {
	return _gamepadDevices.isLocked(name);
}

bool DeviceManager::isGamepadDeviceLockedAt(std::size_t index) const {
	return _gamepadDevices.isLockedAt(index);
}

DeviceResourceStatus DeviceManager::gamepadDeviceStatusAt(std::size_t index) const {
	return _gamepadDevices.statusAt(index);
}

DeviceResourceLock DeviceManager::lockGamepadDevice(
    std::string_view name,
    DeviceResourcePriority priority,
    std::string_view owner) {
	auto const index = _gamepadDevices.findIndex(name);
	if (index == detail::invalidDeviceResourceIndex || !_gamepadDevices.tryLock(index, priority, owner)) {
		return {};
	}
	return DeviceResourceLock{*this, DeviceResourceLock::Kind::GamepadDevice, index};
}

Sensor* DeviceManager::sensorDevice(std::string_view name) {
	auto* descriptor = _sensorDevices.find(name);
	return descriptor != nullptr ? descriptor->device : nullptr;
}

Sensor const* DeviceManager::sensorDevice(std::string_view name) const {
	auto const* descriptor = _sensorDevices.find(name);
	return descriptor != nullptr ? descriptor->device : nullptr;
}

Sensor* DeviceManager::primarySensorDevice() {
	return _sensorDevices.count() > 0 ? _sensorDevices.at(0).device : nullptr;
}

Sensor const* DeviceManager::primarySensorDevice() const {
	return _sensorDevices.count() > 0 ? _sensorDevices.at(0).device : nullptr;
}

std::size_t DeviceManager::sensorDeviceCount() const {
	return _sensorDevices.count();
}

SensorDeviceDescriptor const& DeviceManager::sensorDeviceAt(std::size_t index) const {
	return _sensorDevices.at(index);
}

bool DeviceManager::isSensorDeviceLocked(std::string_view name) const {
	return _sensorDevices.isLocked(name);
}

bool DeviceManager::isSensorDeviceLockedAt(std::size_t index) const {
	return _sensorDevices.isLockedAt(index);
}

DeviceResourceStatus DeviceManager::sensorDeviceStatusAt(std::size_t index) const {
	return _sensorDevices.statusAt(index);
}

DeviceResourceLock DeviceManager::lockSensorDevice(
    std::string_view name,
    DeviceResourcePriority priority,
    std::string_view owner) {
	auto const index = _sensorDevices.findIndex(name);
	if (index == detail::invalidDeviceResourceIndex || !_sensorDevices.tryLock(index, priority, owner)) {
		return {};
	}
	return DeviceResourceLock{*this, DeviceResourceLock::Kind::SensorDevice, index};
}

InfraredTransmitter* DeviceManager::infraredDevice(std::string_view name) {
	auto* descriptor = _infraredDevices.find(name);
	return descriptor != nullptr ? descriptor->device : nullptr;
}

InfraredTransmitter const* DeviceManager::infraredDevice(std::string_view name) const {
	auto const* descriptor = _infraredDevices.find(name);
	return descriptor != nullptr ? descriptor->device : nullptr;
}

InfraredTransmitter* DeviceManager::primaryInfraredDevice() {
	return _infraredDevices.count() > 0 ? _infraredDevices.at(0).device : nullptr;
}

InfraredTransmitter const* DeviceManager::primaryInfraredDevice() const {
	return _infraredDevices.count() > 0 ? _infraredDevices.at(0).device : nullptr;
}

std::size_t DeviceManager::infraredDeviceCount() const {
	return _infraredDevices.count();
}

InfraredDeviceDescriptor const& DeviceManager::infraredDeviceAt(std::size_t index) const {
	return _infraredDevices.at(index);
}

bool DeviceManager::isInfraredDeviceLocked(std::string_view name) const {
	return _infraredDevices.isLocked(name);
}

bool DeviceManager::isInfraredDeviceLockedAt(std::size_t index) const {
	return _infraredDevices.isLockedAt(index);
}

DeviceResourceStatus DeviceManager::infraredDeviceStatusAt(std::size_t index) const {
	return _infraredDevices.statusAt(index);
}

DeviceResourceLock DeviceManager::lockInfraredDevice(
    std::string_view name,
    DeviceResourcePriority priority,
    std::string_view owner) {
	auto const index = _infraredDevices.findIndex(name);
	if (index == detail::invalidDeviceResourceIndex || !_infraredDevices.tryLock(index, priority, owner)) {
		return {};
	}
	return DeviceResourceLock{*this, DeviceResourceLock::Kind::InfraredDevice, index};
}

PicoCalcSouthbridgeRuntimeService* DeviceManager::picoCalcSouthbridgeService(std::string_view name) {
	auto* descriptor = _picoCalcSouthbridgeServices.find(name);
	return descriptor != nullptr ? descriptor->service : nullptr;
}

PicoCalcSouthbridgeRuntimeService const* DeviceManager::picoCalcSouthbridgeService(std::string_view name) const {
	auto const* descriptor = _picoCalcSouthbridgeServices.find(name);
	return descriptor != nullptr ? descriptor->service : nullptr;
}

std::size_t DeviceManager::picoCalcSouthbridgeServiceCount() const {
	return _picoCalcSouthbridgeServices.count();
}

PicoCalcSouthbridgeServiceDescriptor const& DeviceManager::picoCalcSouthbridgeServiceAt(std::size_t index) const {
	return _picoCalcSouthbridgeServices.at(index);
}

bool DeviceManager::isPicoCalcSouthbridgeServiceLocked(std::string_view name) const {
	return _picoCalcSouthbridgeServices.isLocked(name);
}

bool DeviceManager::isPicoCalcSouthbridgeServiceLockedAt(std::size_t index) const {
	return _picoCalcSouthbridgeServices.isLockedAt(index);
}

DeviceResourceStatus DeviceManager::picoCalcSouthbridgeServiceStatusAt(std::size_t index) const {
	return _picoCalcSouthbridgeServices.statusAt(index);
}

DeviceResourceLock DeviceManager::lockPicoCalcSouthbridgeService(
    std::string_view name,
    DeviceResourcePriority priority,
    std::string_view owner) {
	auto const index = _picoCalcSouthbridgeServices.findIndex(name);
	if (index == detail::invalidDeviceResourceIndex || !_picoCalcSouthbridgeServices.tryLock(index, priority, owner)) {
		return {};
	}
	return DeviceResourceLock{*this, DeviceResourceLock::Kind::PicoCalcSouthbridgeService, index};
}

void DeviceManager::_release(DeviceResourceLock::Kind kind, std::size_t index) {
	switch (kind) {
		case DeviceResourceLock::Kind::I2cBus:
			_i2cBuses.release(index);
			break;
		case DeviceResourceLock::Kind::BlockDevice:
			_blockDevices.release(index);
			break;
		case DeviceResourceLock::Kind::DisplayDevice:
			_displayDevices.release(index);
			break;
		case DeviceResourceLock::Kind::InputDevice:
			_inputDevices.release(index);
			break;
		case DeviceResourceLock::Kind::PointingDevice:
			_pointingDevices.release(index);
			break;
		case DeviceResourceLock::Kind::GamepadDevice:
			_gamepadDevices.release(index);
			break;
		case DeviceResourceLock::Kind::SensorDevice:
			_sensorDevices.release(index);
			break;
		case DeviceResourceLock::Kind::InfraredDevice:
			_infraredDevices.release(index);
			break;
		case DeviceResourceLock::Kind::PicoCalcSouthbridgeService:
			_picoCalcSouthbridgeServices.release(index);
			break;
		case DeviceResourceLock::Kind::None:
			break;
	}
}

DeviceManager& deviceManager() {
	static DeviceManager manager;
	return manager;
}

} // namespace lexxos::devices
