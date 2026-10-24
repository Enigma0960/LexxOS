#include <lexxos/devices/touch/gt911_touch_device.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <lexxos/platform/platform.h>

namespace lexxos::devices {
namespace {

	constexpr std::uint16_t gt911ProductIdRegister = 0x8140;
	constexpr std::uint16_t gt911StatusRegister = 0x814E;
	constexpr std::uint16_t gt911PointDataRegister = 0x814F;
	constexpr std::uint8_t gt911StatusBufferReady = 0x80;
	constexpr std::uint8_t gt911ContactCountMask = 0x0F;
	constexpr std::uint8_t gt911ContactBytes = 8;
	constexpr std::uint8_t gt911MaxContacts = 5;
	constexpr std::uint8_t gt911FallbackAddresses[] = {0x5D, 0x14};

	[[nodiscard]] std::int32_t clampInt(std::int32_t value, std::int32_t low, std::int32_t high) {
		if (low > high) {
			auto const temporary = low;
			low = high;
			high = temporary;
		}
		if (value < low) {
			return low;
		}
		if (value > high) {
			return high;
		}
		return value;
	}

	[[nodiscard]] std::int32_t mapAxis(
	    std::int32_t value,
	    std::int32_t rawMin,
	    std::int32_t rawMax,
	    std::int32_t displayExtent,
	    bool invert) {
		if (displayExtent <= 1 || rawMin == rawMax) {
			return 0;
		}
		if (rawMin > rawMax) {
			auto const temporary = rawMin;
			rawMin = rawMax;
			rawMax = temporary;
		}
		auto const clamped = clampInt(value, rawMin, rawMax);
		auto const range = static_cast<std::int64_t>(rawMax - rawMin);
		auto const scaled = (static_cast<std::int64_t>(clamped - rawMin) * (displayExtent - 1) + range / 2) / range;
		auto mapped = static_cast<std::int32_t>(scaled);
		if (invert) {
			mapped = displayExtent - 1 - mapped;
		}
		return clampInt(mapped, 0, displayExtent - 1);
	}

	[[nodiscard]] std::uint16_t readLe16(std::uint8_t const* data) {
		return static_cast<std::uint16_t>(data[0] | (static_cast<std::uint16_t>(data[1]) << 8U));
	}

} // namespace

Gt911TouchDevice::Gt911TouchDevice(
    I2cBus& bus,
    lexxos::platform::TouchPanelConfig config,
    lexxos::platform::PlatformServices const& platform)
    : _bus(bus)
    , _config(config)
    , _platform(platform) {
	_setDefaultCalibration();
}

bool Gt911TouchDevice::initialize() {
	if (!_config.isValid() || _config.controller != lexxos::platform::TouchPanelKind::Gt911 || !_bus.isReady()) {
		return false;
	}

	if (_tryAddress(_config.address)) {
		_ready = true;
		return true;
	}

	for (auto const address : gt911FallbackAddresses) {
		if (address == _config.address) {
			continue;
		}
		if (_tryAddress(address)) {
			_ready = true;
			return true;
		}
	}

	return false;
}

std::string_view Gt911TouchDevice::name() const {
	return _config.name != nullptr ? _config.name : "gt911-touch";
}

bool Gt911TouchDevice::isReady() const {
	return _ready;
}

PointingDeviceState Gt911TouchDevice::state() const {
	auto const touch = touchState();
	PointingDeviceState result{};
	result.connected = touch.connected;
	result.reportCount = touch.reportCount;
	result.lastReportTimestampMs = touch.lastReportTimestampMs;
	if (touch.contactCount != 0 && touch.contacts[0].active) {
		result.x = touch.contacts[0].x;
		result.y = touch.contacts[0].y;
		result.buttons = PointingButtonLeft;
	}
	return result;
}

bool Gt911TouchDevice::supportsTouch() const {
	return true;
}

TouchDeviceState Gt911TouchDevice::touchState() const {
	_poll();
	return _touch;
}

TouchCalibration Gt911TouchDevice::touchCalibration() const {
	return _touch.calibration;
}

bool Gt911TouchDevice::applyTouchCalibration(TouchCalibration const& calibration) {
	if (!calibration.valid || calibration.rawMinX == calibration.rawMaxX || calibration.rawMinY == calibration.rawMaxY || calibration.displayWidth <= 1 || calibration.displayHeight <= 1) {
		return false;
	}
	auto normalized = calibration;
	if (normalized.rawMinX > normalized.rawMaxX) {
		auto const temporary = normalized.rawMinX;
		normalized.rawMinX = normalized.rawMaxX;
		normalized.rawMaxX = temporary;
	}
	if (normalized.rawMinY > normalized.rawMaxY) {
		auto const temporary = normalized.rawMinY;
		normalized.rawMinY = normalized.rawMaxY;
		normalized.rawMaxY = temporary;
	}
	_touch.calibration = normalized;
	return true;
}

void Gt911TouchDevice::resetPosition() {
	_touch.contactCount = 0;
	for (auto& contact : _touch.contacts) {
		contact = {};
	}
}

bool Gt911TouchDevice::_tryAddress(std::uint8_t address) {
	if (address == 0 || !_bus.probe(address)) {
		return false;
	}

	std::array<std::uint8_t, 4> product{};
	if (!_readRegister(address, gt911ProductIdRegister, product.data(), product.size())) {
		return false;
	}
	std::memcpy(_productId, product.data(), product.size());
	_productId[4] = '\0';
	_address = address;
	return true;
}

bool Gt911TouchDevice::_readRegister(
    std::uint8_t address,
    std::uint16_t reg,
    std::uint8_t* data,
    std::size_t size) const {
	if (address == 0 || data == nullptr || size == 0) {
		return false;
	}
	std::uint8_t pointer[] = {
	    static_cast<std::uint8_t>(reg >> 8U),
	    static_cast<std::uint8_t>(reg & 0xFFU),
	};
	I2cBusTransaction transaction{_bus};
	if (!transaction) {
		return false;
	}
	return _bus.write(address, pointer, sizeof(pointer)) && _bus.read(address, data, size);
}

bool Gt911TouchDevice::_readRegister(std::uint16_t reg, std::uint8_t* data, std::size_t size) const {
	return _readRegister(_address, reg, data, size);
}

bool Gt911TouchDevice::_writeRegister(std::uint16_t reg, std::uint8_t const* data, std::size_t size) const {
	if (_address == 0 || data == nullptr || size == 0 || size + 2U > 18U) {
		return false;
	}
	std::array<std::uint8_t, 18> buffer{};
	buffer[0] = static_cast<std::uint8_t>(reg >> 8U);
	buffer[1] = static_cast<std::uint8_t>(reg & 0xFFU);
	std::memcpy(buffer.data() + 2, data, size);
	return _bus.write(_address, buffer.data(), size + 2U);
}

void Gt911TouchDevice::_poll() const {
	if (!_ready || _address == 0) {
		_touch.connected = false;
		return;
	}

	std::uint8_t status = 0;
	if (!_readRegister(gt911StatusRegister, &status, 1)) {
		_touch.connected = false;
		return;
	}
	_touch.connected = true;
	_touch.maxContacts = gt911MaxContacts;

	if ((status & gt911StatusBufferReady) == 0) {
		return;
	}

	auto contactCount = static_cast<std::uint8_t>(status & gt911ContactCountMask);
	if (contactCount > gt911MaxContacts || contactCount > maxTouchContacts) {
		contactCount = 0;
	}

	std::array<std::uint8_t, gt911MaxContacts * gt911ContactBytes> data{};
	if (contactCount != 0
	    && !_readRegister(gt911PointDataRegister, data.data(), static_cast<std::size_t>(contactCount) * gt911ContactBytes)) {
		_touch.connected = false;
		return;
	}

	for (auto& contact : _touch.contacts) {
		contact = {};
	}
	_touch.contactCount = contactCount;
	for (std::uint8_t index = 0; index < contactCount; ++index) {
		auto const* point = data.data() + static_cast<std::size_t>(index) * gt911ContactBytes;
		auto const id = point[0];
		auto const rawX = readLe16(point + 1);
		auto const rawY = readLe16(point + 3);
		auto const size = readLe16(point + 5);
		_touch.contacts[index] = _mapContact(id, rawX, rawY, size);
	}

	++_touch.reportCount;
	_touch.lastReportTimestampMs = _uptimeMs();
	std::uint8_t clear = 0;
	(void) _writeRegister(gt911StatusRegister, &clear, 1);
}

void Gt911TouchDevice::_setDefaultCalibration() {
	_touch.connected = false;
	_touch.maxContacts = gt911MaxContacts;
	_touch.calibration = {
	    .valid = true,
	    .swapXY = _config.swapXY,
	    .invertX = _config.invertX,
	    .invertY = _config.invertY,
	    .rawMinX = 0,
	    .rawMaxX = static_cast<std::int32_t>(_config.rawWidth > 0 ? _config.rawWidth - 1U : 799U),
	    .rawMinY = 0,
	    .rawMaxY = static_cast<std::int32_t>(_config.rawHeight > 0 ? _config.rawHeight - 1U : 479U),
	    .displayWidth = static_cast<std::int32_t>(_config.rawWidth > 0 ? _config.rawWidth : 800U),
	    .displayHeight = static_cast<std::int32_t>(_config.rawHeight > 0 ? _config.rawHeight : 480U),
	};
}

TouchContact Gt911TouchDevice::_mapContact(
    std::uint8_t id,
    std::uint16_t rawX,
    std::uint16_t rawY,
    std::uint16_t size) const {
	auto const& calibration = _touch.calibration;
	auto const axisX = calibration.swapXY ? static_cast<std::int32_t>(rawY) : static_cast<std::int32_t>(rawX);
	auto const axisY = calibration.swapXY ? static_cast<std::int32_t>(rawX) : static_cast<std::int32_t>(rawY);
	return {
	    .active = true,
	    .id = id,
	    .x = mapAxis(axisX, calibration.rawMinX, calibration.rawMaxX, calibration.displayWidth, calibration.invertX),
	    .y = mapAxis(axisY, calibration.rawMinY, calibration.rawMaxY, calibration.displayHeight, calibration.invertY),
	    .rawX = rawX,
	    .rawY = rawY,
	    .size = size,
	};
}

std::uint32_t Gt911TouchDevice::_uptimeMs() const {
	if (_platform.clock.uptimeMs != nullptr) {
		return _platform.clock.uptimeMs();
	}
	return lexxos::platform::uptimeMs();
}

} // namespace lexxos::devices
