#include <lexxos/devices/touch/ft5x06_touch_device.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <lexxos/platform/platform.h>

namespace lexxos::devices {
namespace {

	constexpr std::uint8_t ft5x06StatusRegister = 0x02;
	constexpr std::uint8_t ft5x06FirstPointRegister = 0x03;
	constexpr std::uint8_t ft5x06ChipIdRegister = 0xA3;
	constexpr std::uint8_t ft5x06MaxContacts = 5;
	constexpr std::uint8_t ft5x06PointBytes = 6;
	constexpr std::uint8_t ft5x06FallbackAddress = 0x38;

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
		auto mapped = static_cast<std::int32_t>(
		    (static_cast<std::int64_t>(clamped - rawMin) * (displayExtent - 1) + range / 2) / range);
		if (invert) {
			mapped = displayExtent - 1 - mapped;
		}
		return clampInt(mapped, 0, displayExtent - 1);
	}

} // namespace

Ft5x06TouchDevice::Ft5x06TouchDevice(
    I2cBus& bus,
    lexxos::platform::TouchPanelConfig config,
    lexxos::platform::PlatformServices const& platform)
    : _bus(bus)
    , _config(config)
    , _platform(platform) {
	_setDefaultCalibration();
}

bool Ft5x06TouchDevice::initialize() {
	if (!_config.isValid() || _config.controller != lexxos::platform::TouchPanelKind::FocalTechFt5x06 || !_bus.isReady()) {
		return false;
	}
	if (_tryAddress(_config.address)) {
		_ready = true;
		return true;
	}
	if (_config.address != ft5x06FallbackAddress && _tryAddress(ft5x06FallbackAddress)) {
		_ready = true;
		return true;
	}
	return false;
}

std::string_view Ft5x06TouchDevice::name() const {
	return _config.name != nullptr ? _config.name : "ft5x06-touch";
}

bool Ft5x06TouchDevice::isReady() const {
	return _ready;
}

PointingDeviceState Ft5x06TouchDevice::state() const {
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

bool Ft5x06TouchDevice::supportsTouch() const {
	return true;
}

TouchDeviceState Ft5x06TouchDevice::touchState() const {
	_poll();
	return _touch;
}

TouchCalibration Ft5x06TouchDevice::touchCalibration() const {
	return _touch.calibration;
}

bool Ft5x06TouchDevice::applyTouchCalibration(TouchCalibration const& calibration) {
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

void Ft5x06TouchDevice::resetPosition() {
	_touch.contactCount = 0;
	for (auto& contact : _touch.contacts) {
		contact = {};
	}
}

bool Ft5x06TouchDevice::_tryAddress(std::uint8_t address) {
	if (address == 0 || !_bus.probe(address)) {
		return false;
	}

	std::uint8_t ignored = 0;
	if (!_readRegister(address, ft5x06StatusRegister, &ignored, 1)) {
		return false;
	}
	(void) _readRegister(address, ft5x06ChipIdRegister, &ignored, 1);
	_address = address;
	return true;
}

bool Ft5x06TouchDevice::_readRegister(
    std::uint8_t address,
    std::uint8_t reg,
    std::uint8_t* data,
    std::size_t size) const {
	if (address == 0 || data == nullptr || size == 0) {
		return false;
	}
	I2cBusTransaction transaction{_bus};
	if (!transaction) {
		return false;
	}
	return _bus.write(address, &reg, 1) && _bus.read(address, data, size);
}

bool Ft5x06TouchDevice::_readRegister(std::uint8_t reg, std::uint8_t* data, std::size_t size) const {
	return _readRegister(_address, reg, data, size);
}

void Ft5x06TouchDevice::_poll() const {
	if (!_ready || _address == 0) {
		_touch.connected = false;
		return;
	}

	std::uint8_t status = 0;
	if (!_readRegister(ft5x06StatusRegister, &status, 1)) {
		_touch.connected = false;
		return;
	}

	auto contactCount = static_cast<std::uint8_t>(status & 0x0FU);
	if (contactCount > ft5x06MaxContacts || contactCount > maxTouchContacts) {
		contactCount = 0;
	}

	std::array<std::uint8_t, ft5x06MaxContacts * ft5x06PointBytes> data{};
	if (contactCount != 0
	    && !_readRegister(ft5x06FirstPointRegister, data.data(), static_cast<std::size_t>(contactCount) * ft5x06PointBytes)) {
		_touch.connected = false;
		return;
	}

	for (auto& contact : _touch.contacts) {
		contact = {};
	}
	_touch.connected = true;
	_touch.maxContacts = ft5x06MaxContacts;
	_touch.contactCount = contactCount;
	for (std::uint8_t index = 0; index < contactCount; ++index) {
		auto const* point = data.data() + static_cast<std::size_t>(index) * ft5x06PointBytes;
		auto const rawX = static_cast<std::uint16_t>(((point[0] & 0x0FU) << 8U) | point[1]);
		auto const rawY = static_cast<std::uint16_t>(((point[2] & 0x0FU) << 8U) | point[3]);
		auto const id = static_cast<std::uint8_t>((point[2] >> 4U) & 0x0FU);
		auto const size = static_cast<std::uint16_t>(point[4]);
		_touch.contacts[index] = _mapContact(id, rawX, rawY, size);
	}

	++_touch.reportCount;
	_touch.lastReportTimestampMs = _uptimeMs();
}

void Ft5x06TouchDevice::_setDefaultCalibration() {
	_touch.connected = false;
	_touch.maxContacts = ft5x06MaxContacts;
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

TouchContact Ft5x06TouchDevice::_mapContact(
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

std::uint32_t Ft5x06TouchDevice::_uptimeMs() const {
	if (_platform.clock.uptimeMs != nullptr) {
		return _platform.clock.uptimeMs();
	}
	return lexxos::platform::uptimeMs();
}

} // namespace lexxos::devices
