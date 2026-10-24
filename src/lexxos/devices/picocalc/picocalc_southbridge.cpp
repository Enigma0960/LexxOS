#include <lexxos/devices/picocalc/picocalc_southbridge.h>

#include <array>

#include <lexxos/devices/bus/i2c_bus.h>

#if defined(LEXXOS_KERNEL_FREERTOS)
	#include <FreeRTOS.h>
	#include <task.h>
#endif

#if __has_include(<pico/time.h>)
	#include <pico/time.h>
	#define LEXXOS_PICOCALC_HAS_PICO_TIME 1
#endif

namespace lexxos::devices {
namespace {

#ifndef LEXXOS_PICOCALC_REGISTER_RESPONSE_DELAY_MS
	#define LEXXOS_PICOCALC_REGISTER_RESPONSE_DELAY_MS 2
#endif

	constexpr std::uint32_t registerResponseDelayMs = LEXXOS_PICOCALC_REGISTER_RESPONSE_DELAY_MS;

	constexpr std::uint32_t jcsRtcWriteReadbackToleranceSeconds = 2;

	void sleepRegisterResponseDelay() {
		if constexpr (registerResponseDelayMs == 0) {
			return;
		}

#if defined(LEXXOS_KERNEL_FREERTOS)
		if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
			auto ticks = pdMS_TO_TICKS(registerResponseDelayMs);
			if (ticks == 0) {
				ticks = 1;
			}
			vTaskDelay(ticks);
			return;
		}
#endif
#if defined(LEXXOS_PICOCALC_HAS_PICO_TIME)
		sleep_ms(registerResponseDelayMs);
#else
		static_cast<void>(registerResponseDelayMs);
#endif
	}

	void setWriteState(PicoCalcSouthbridgeCommandState* state, PicoCalcSouthbridgeCommandState value) {
		if (state != nullptr) {
			*state = value;
		}
	}

	bool secondsSince2000(lexxos::devices::rtc::DateTime const& value, std::uint64_t& output) {
		if (!lexxos::devices::rtc::isValidDate(value)) {
			return false;
		}

		std::uint64_t days = 0;
		for (std::uint16_t year = 2000U; year < value.year; ++year) {
			days += lexxos::devices::rtc::isLeapYear(year) ? 366U : 365U;
		}
		for (std::uint8_t month = 1U; month < value.month; ++month) {
			days += lexxos::devices::rtc::daysInMonth(value.year, month);
		}
		days += static_cast<std::uint64_t>(value.day - 1U);

		output = days * 86400U
		    + static_cast<std::uint64_t>(value.hour) * 3600U
		    + static_cast<std::uint64_t>(value.minute) * 60U
		    + static_cast<std::uint64_t>(value.second);
		return true;
	}

	bool dateTimeCloseTo(lexxos::devices::rtc::DateTime const& actual,
	    lexxos::devices::rtc::DateTime const& expected,
	    std::uint32_t toleranceSeconds) {
		std::uint64_t actualSeconds = 0;
		std::uint64_t expectedSeconds = 0;
		if (!secondsSince2000(actual, actualSeconds) || !secondsSince2000(expected, expectedSeconds)) {
			return false;
		}
		std::uint64_t const delta = actualSeconds > expectedSeconds
		    ? actualSeconds - expectedSeconds
		    : expectedSeconds - actualSeconds;
		return delta <= toleranceSeconds;
	}

} // namespace

std::uint16_t PicoCalcKeyboardState::raw() const {
	return static_cast<std::uint16_t>((state << 8U) | value);
}

std::uint8_t PicoCalcKeyboardState::fifoCount() const {
	return static_cast<std::uint8_t>(state & PicoCalcSouthbridge::keyboardFifoCountMask);
}

std::uint8_t PicoCalcKeyboardState::legacyFifoCount() const {
	return static_cast<std::uint8_t>(value & PicoCalcSouthbridge::keyboardFifoCountMask);
}

bool PicoCalcKeyboardState::capsLock() const {
	return (state & PicoCalcSouthbridge::keyboardCapsLockMask) != 0;
}

bool PicoCalcKeyboardState::numLock() const {
	return (state & PicoCalcSouthbridge::keyboardNumLockMask) != 0;
}

std::uint16_t PicoCalcKeyboardFifoEvent::raw() const {
	return static_cast<std::uint16_t>((status << 8U) | code);
}

bool PicoCalcKeyboardFifoEvent::isEmpty() const {
	return status == 0 && code == 0;
}

bool PicoCalcBatteryStatus::charging() const {
	return (raw & PicoCalcSouthbridge::batteryChargingMask) != 0;
}

std::uint8_t PicoCalcBatteryStatus::value() const {
	return static_cast<std::uint8_t>(raw & PicoCalcSouthbridge::batteryValueMask);
}

bool PicoCalcJcsFirmwareInfo::isJcs() const {
	return type == PicoCalcSouthbridge::jcsFirmwareType;
}

std::uint8_t PicoCalcJcsFirmwareInfo::majorVersion() const {
	return static_cast<std::uint8_t>((version >> 4U) & 0x0FU);
}

std::uint8_t PicoCalcJcsFirmwareInfo::minorVersion() const {
	return static_cast<std::uint8_t>(version & 0x0FU);
}

bool PicoCalcJcsRtcConfig::alarmEnabled() const {
	return (raw & PicoCalcSouthbridge::jcsRtcAlarmEnabledMask) != 0U;
}

bool PicoCalcJcsRtcConfig::alarmRearm() const {
	return (raw & PicoCalcSouthbridge::jcsRtcAlarmRearmMask) != 0U;
}

bool PicoCalcSouthbridgeSnapshot::has(std::uint16_t flag) const {
	return (validMask & flag) != 0;
}

bool PicoCalcSouthbridgeSnapshot::empty() const {
	return validMask == 0;
}

std::uint8_t PicoCalcSouthbridgeSnapshot::validFieldCount() const {
	std::uint16_t mask = validMask;
	std::uint8_t count = 0;
	while (mask != 0) {
		if ((mask & 1U) != 0) {
			++count;
		}
		mask = static_cast<std::uint16_t>(mask >> 1U);
	}
	return count;
}

void PicoCalcSouthbridgeSnapshot::clear() {
	*this = {};
}

void PicoCalcSouthbridgeSnapshot::setValid(std::uint16_t flag) {
	validMask = static_cast<std::uint16_t>(validMask | flag);
}

void PicoCalcSouthbridgeSnapshot::setKeyboardState(PicoCalcKeyboardState const& state) {
	keyboardState = state;
	setValid(validKeyboardState);
}

void PicoCalcSouthbridgeSnapshot::setKeyboardState(std::uint8_t state, std::uint8_t value) {
	PicoCalcKeyboardState keyboardState{};
	keyboardState.state = state;
	keyboardState.value = value;
	setKeyboardState(keyboardState);
}

PicoCalcSouthbridge::PicoCalcSouthbridge(I2cBus& bus, std::uint8_t address)
    : _bus(bus)
    , _address(address) {
}

std::uint8_t PicoCalcSouthbridge::address() const {
	return _address;
}

bool PicoCalcSouthbridge::probe() const {
	return _bus.probe(_address);
}

bool PicoCalcSouthbridge::isReady() const {
	return _bus.isReady();
}

bool PicoCalcSouthbridge::readVersion(std::uint8_t& value) const {
	return _readValue(registerVersion, value);
}

bool PicoCalcSouthbridge::readConfig(std::uint8_t& value) const {
	return _readValue(registerConfig, value);
}

bool PicoCalcSouthbridge::setConfig(std::uint8_t value, std::uint8_t* acceptedValue, PicoCalcSouthbridgeCommandState* state) const {
	return _writeValue(registerConfig, value, acceptedValue, state);
}

bool PicoCalcSouthbridge::readInterruptStatus(std::uint8_t& value) const {
	return _readValue(registerInterruptStatus, value);
}

bool PicoCalcSouthbridge::readDebounce(std::uint8_t& value) const {
	return _readValue(registerDebounce, value);
}

bool PicoCalcSouthbridge::setDebounce(std::uint8_t value, std::uint8_t* acceptedValue, PicoCalcSouthbridgeCommandState* state) const {
	return _writeValue(registerDebounce, value, acceptedValue, state);
}

bool PicoCalcSouthbridge::readPollingFrequency(std::uint8_t& value) const {
	return _readValue(registerPollingFrequency, value);
}

bool PicoCalcSouthbridge::setPollingFrequency(std::uint8_t value, std::uint8_t* acceptedValue, PicoCalcSouthbridgeCommandState* state) const {
	return _writeValue(registerPollingFrequency, value, acceptedValue, state);
}

bool PicoCalcSouthbridge::readKeyboardState(PicoCalcKeyboardState& state) const {
	std::uint8_t response[2]{};
	if (!_readPair(registerKeyboardState, response)) {
		return false;
	}

	state.state = response[0];
	state.value = response[1];
	return true;
}

bool PicoCalcSouthbridge::readKeyboardFifo(PicoCalcKeyboardFifoEvent& event) const {
	std::uint8_t response[2]{};
	if (!_readPair(registerKeyboardFifo, response)) {
		return false;
	}

	event.status = response[0];
	event.code = response[1];
	return true;
}

bool PicoCalcSouthbridge::readBattery(PicoCalcBatteryStatus& status) const {
	std::uint8_t value = 0;
	if (!_readValue(registerBattery, value)) {
		return false;
	}

	status.raw = value;
	return true;
}

bool PicoCalcSouthbridge::readLcdBacklight(std::uint8_t& value) const {
	return _readValue(registerLcdBacklight, value);
}

bool PicoCalcSouthbridge::setLcdBacklight(std::uint8_t value, std::uint8_t* acceptedValue, PicoCalcSouthbridgeCommandState* state) const {
	return _writeValue(registerLcdBacklight, value, acceptedValue, state);
}

bool PicoCalcSouthbridge::readKeyboardBacklight(std::uint8_t& value) const {
	return _readValue(registerKeyboardBacklight, value);
}

bool PicoCalcSouthbridge::setKeyboardBacklight(std::uint8_t value, std::uint8_t* acceptedValue, PicoCalcSouthbridgeCommandState* state) const {
	return _writeValue(registerKeyboardBacklight, value, acceptedValue, state);
}

bool PicoCalcSouthbridge::readC64Matrix(std::uint8_t& value) const {
	return _readValue(registerC64Matrix, value);
}

bool PicoCalcSouthbridge::readC64Joystick(std::uint8_t& value) const {
	return _readValue(registerC64Joystick, value);
}

bool PicoCalcSouthbridge::readPowerOffSupport(std::uint8_t& value) const {
#if defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
	value = jcsPowerSleepMask;
	return true;
#else
	return _readValue(registerPowerOff, value);
#endif
}

bool PicoCalcSouthbridge::readFirmwareType(std::uint8_t& value) const {
	return _readValue(registerFirmwareType, value);
}

bool PicoCalcSouthbridge::readJcsFirmwareInfo(PicoCalcJcsFirmwareInfo& info) const {
	PicoCalcJcsFirmwareInfo value{};
	if (!readFirmwareType(value.type) || !readVersion(value.version)) {
		return false;
	}
	info = value;
	return true;
}

bool PicoCalcSouthbridge::readJcsRtcConfig(PicoCalcJcsRtcConfig& config) const {
	std::uint8_t value = 0;
	if (!_readValue(registerJcsRtcConfig, value)) {
		return false;
	}
	config.raw = value;
	return true;
}

bool PicoCalcSouthbridge::setJcsRtcConfig(
    PicoCalcJcsRtcConfig config,
    PicoCalcJcsRtcConfig* acceptedConfig,
    PicoCalcSouthbridgeCommandState* state) const {
	std::uint8_t acceptedValue = 0;
	bool const ok = _writeValue(registerJcsRtcConfig, config.raw, &acceptedValue, state);
	if (ok && acceptedConfig != nullptr) {
		acceptedConfig->raw = acceptedValue;
	}
	return ok;
}

bool PicoCalcSouthbridge::readJcsRtcDateTime(lexxos::devices::rtc::DateTime& value) const {
	lexxos::devices::rtc::DateTime dateTime{};
	if (!_readJcsRtcDate(registerJcsRtcDate, dateTime) || !_readJcsRtcTime(registerJcsRtcTime, dateTime)) {
		return false;
	}
	if (dateTime.weekday == 0U) {
		dateTime.weekday = lexxos::devices::rtc::weekdayIso(dateTime);
	}
	if (!lexxos::devices::rtc::isValidDate(dateTime)) {
		return false;
	}
	value = dateTime;
	return true;
}

bool PicoCalcSouthbridge::setJcsRtcDateTime(
    lexxos::devices::rtc::DateTime const& value,
    PicoCalcSouthbridgeCommandState* state) const {
	PicoCalcSouthbridgeCommandState dateState = PicoCalcSouthbridgeCommandState::Failed;
	if (!_writeJcsRtcDate(registerJcsRtcDate, value, &dateState)) {
		setWriteState(state, dateState);
		return false;
	}
	PicoCalcSouthbridgeCommandState timeState = PicoCalcSouthbridgeCommandState::Failed;
	if (_writeJcsRtcTime(registerJcsRtcTime, value, &timeState)) {
		setWriteState(state, timeState);
		return true;
	}

	lexxos::devices::rtc::DateTime readback{};
	if (readJcsRtcDateTime(readback) && dateTimeCloseTo(readback, value, jcsRtcWriteReadbackToleranceSeconds)) {
		setWriteState(state, PicoCalcSouthbridgeCommandState::Ok);
		return true;
	}

	setWriteState(state, timeState);
	return false;
}

bool PicoCalcSouthbridge::readJcsRtcAlarm(lexxos::devices::rtc::DateTime& value) const {
	lexxos::devices::rtc::DateTime dateTime{};
	if (!_readJcsRtcDate(registerJcsRtcAlarmDate, dateTime) || !_readJcsRtcTime(registerJcsRtcAlarmTime, dateTime)) {
		return false;
	}
	if (dateTime.weekday == 0U) {
		dateTime.weekday = lexxos::devices::rtc::weekdayIso(dateTime);
	}
	if (!lexxos::devices::rtc::isValidDate(dateTime)) {
		return false;
	}
	value = dateTime;
	return true;
}

bool PicoCalcSouthbridge::setJcsRtcAlarm(
    lexxos::devices::rtc::DateTime const& value,
    PicoCalcSouthbridgeCommandState* state) const {
	PicoCalcSouthbridgeCommandState dateState = PicoCalcSouthbridgeCommandState::Failed;
	if (!_writeJcsRtcDate(registerJcsRtcAlarmDate, value, &dateState)) {
		setWriteState(state, dateState);
		return false;
	}
	return _writeJcsRtcTime(registerJcsRtcAlarmTime, value, state);
}

bool PicoCalcSouthbridge::requestReset(std::uint8_t delaySeconds, PicoCalcSouthbridgeCommandState* state) const {
#if defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
	(void) delaySeconds;
	return _writeValue(registerReset, jcsResetPicoMask, nullptr, state);
#else
	setWriteState(state, PicoCalcSouthbridgeCommandState::Failed);

	I2cBusTransaction transaction{_bus};
	if (!transaction) {
		return false;
	}

	std::uint8_t command[2]{
	    static_cast<std::uint8_t>(registerReset | registerWriteMask),
	    delaySeconds,
	};
	if (!_bus.write(_address, command, sizeof(command))) {
		return false;
	}

	setWriteState(state, PicoCalcSouthbridgeCommandState::Ok);
	return true;
#endif
}

bool PicoCalcSouthbridge::requestPowerOff(PicoCalcSouthbridgeCommandState* state) const {
#if defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
	return _writeValue(registerPowerOff, jcsPowerSleepMask, nullptr, state);
#else
	return _writeValue(registerPowerOff, 1, nullptr, state);
#endif
}

bool PicoCalcSouthbridge::readSnapshot(PicoCalcSouthbridgeSnapshot& snapshot) const {
	snapshot.clear();

	std::uint8_t value = 0;
#if defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
	bool jcsExtendedRegisters = false;
	if (readFirmwareType(value)) {
		snapshot.firmwareType = value;
		snapshot.setValid(PicoCalcSouthbridgeSnapshot::validFirmwareType);
		jcsExtendedRegisters = value == jcsFirmwareType;
	}
#endif

	if (readVersion(value)) {
		snapshot.version = value;
		snapshot.setValid(PicoCalcSouthbridgeSnapshot::validVersion);
	}

	if (readConfig(value)) {
		snapshot.config = value;
		snapshot.setValid(PicoCalcSouthbridgeSnapshot::validConfig);
	}

	if (readInterruptStatus(value)) {
		snapshot.interruptStatus = value;
		snapshot.setValid(PicoCalcSouthbridgeSnapshot::validInterruptStatus);
	}

	if (readDebounce(value)) {
		snapshot.debounce = value;
		snapshot.setValid(PicoCalcSouthbridgeSnapshot::validDebounce);
	}

	if (readPollingFrequency(value)) {
		snapshot.pollingFrequency = value;
		snapshot.setValid(PicoCalcSouthbridgeSnapshot::validPollingFrequency);
	}

	PicoCalcKeyboardState keyboardState{};
	if (readKeyboardState(keyboardState)) {
		snapshot.setKeyboardState(keyboardState);
	}

	PicoCalcBatteryStatus battery{};
	if (readBattery(battery)) {
		snapshot.battery = battery;
		snapshot.setValid(PicoCalcSouthbridgeSnapshot::validBattery);
	}

	if (readLcdBacklight(value)) {
		snapshot.lcdBacklight = value;
		snapshot.setValid(PicoCalcSouthbridgeSnapshot::validLcdBacklight);
	}

	if (readKeyboardBacklight(value)) {
		snapshot.keyboardBacklight = value;
		snapshot.setValid(PicoCalcSouthbridgeSnapshot::validKeyboardBacklight);
	}

	if (readC64Matrix(value)) {
		snapshot.c64Matrix = value;
		snapshot.setValid(PicoCalcSouthbridgeSnapshot::validC64Matrix);
	}

	if (readC64Joystick(value)) {
		snapshot.c64Joystick = value;
		snapshot.setValid(PicoCalcSouthbridgeSnapshot::validC64Joystick);
	}

	if (readPowerOffSupport(value)) {
		snapshot.powerOffSupport = value;
		snapshot.setValid(PicoCalcSouthbridgeSnapshot::validPowerOffSupport);
	}

#if defined(LEXXOS_PICOCALC_KEYBOARD_FIRMWARE_JCS)
	if (jcsExtendedRegisters) {
		PicoCalcJcsRtcConfig rtcConfig{};
		if (readJcsRtcConfig(rtcConfig)) {
			snapshot.jcsRtcConfig = rtcConfig;
			snapshot.setValid(PicoCalcSouthbridgeSnapshot::validJcsRtcConfig);
		}

		lexxos::devices::rtc::DateTime dateTime{};
		if (readJcsRtcDateTime(dateTime)) {
			snapshot.jcsRtcDateTime = dateTime;
			snapshot.setValid(PicoCalcSouthbridgeSnapshot::validJcsRtcDateTime);
		}

		lexxos::devices::rtc::DateTime alarm{};
		if (readJcsRtcAlarm(alarm)) {
			snapshot.jcsRtcAlarm = alarm;
			snapshot.setValid(PicoCalcSouthbridgeSnapshot::validJcsRtcAlarm);
		}
	}
#endif

	return snapshot.validMask != 0;
}

bool PicoCalcSouthbridge::_readPair(std::uint8_t reg, std::uint8_t (&response)[2]) const {
	return _readBytes(reg, response, 2);
}

bool PicoCalcSouthbridge::_readBytes(std::uint8_t reg, std::uint8_t* response, std::uint8_t responseSize) const {
	if (response == nullptr || responseSize == 0U) {
		return false;
	}

	I2cBusTransaction transaction{_bus};
	if (!transaction) {
		return false;
	}

	std::uint8_t command = reg;
	if (!_bus.write(_address, &command, 1)) {
		return false;
	}

	sleepRegisterResponseDelay();
	return _bus.read(_address, response, responseSize);
}

bool PicoCalcSouthbridge::_readValue(std::uint8_t reg, std::uint8_t& value) const {
	std::uint8_t response[2]{};
	if (!_readPair(reg, response)) {
		return false;
	}

	value = response[1];
	return true;
}

bool PicoCalcSouthbridge::_writeValue(
    std::uint8_t reg,
    std::uint8_t value,
    std::uint8_t* acceptedValue,
    PicoCalcSouthbridgeCommandState* state) const {
	std::uint8_t response[2]{};
	if (!_writeBytes(reg, &value, 1, response, 2, state)) {
		return false;
	}

	if (acceptedValue != nullptr) {
		*acceptedValue = response[1];
	}
	return true;
}

bool PicoCalcSouthbridge::_writeBytes(
    std::uint8_t reg,
    std::uint8_t const* data,
    std::uint8_t size,
    std::uint8_t* response,
    std::uint8_t responseSize,
    PicoCalcSouthbridgeCommandState* state) const {
	setWriteState(state, PicoCalcSouthbridgeCommandState::Failed);
	if (data == nullptr || size == 0U || response == nullptr || responseSize == 0U) {
		setWriteState(state, PicoCalcSouthbridgeCommandState::Refused);
		return false;
	}

	std::array<std::uint8_t, 8> command{};
	if (size + 1U > command.size()) {
		setWriteState(state, PicoCalcSouthbridgeCommandState::Refused);
		return false;
	}

	I2cBusTransaction transaction{_bus};
	if (!transaction) {
		return false;
	}

	command[0] = static_cast<std::uint8_t>(reg | registerWriteMask);
	for (std::uint8_t index = 0; index < size; ++index) {
		command[static_cast<std::size_t>(index) + 1U] = data[index];
	}
	if (!_bus.write(_address, command.data(), static_cast<std::size_t>(size + 1U))) {
		return false;
	}

	sleepRegisterResponseDelay();
	bool readOk = false;
	for (std::uint8_t attempt = 0; attempt < writeResponseRetryCount; ++attempt) {
		if (_bus.read(_address, response, responseSize)) {
			readOk = true;
			break;
		}
	}
	if (!readOk) {
		return false;
	}

	if (response[0] != reg) {
		setWriteState(state, PicoCalcSouthbridgeCommandState::Refused);
		return false;
	}

	setWriteState(state, PicoCalcSouthbridgeCommandState::Ok);
	return true;
}

bool PicoCalcSouthbridge::_readJcsRtcDate(std::uint8_t reg, lexxos::devices::rtc::DateTime& value) const {
	std::uint8_t response[5]{};
	if (!_readBytes(reg, response, 5) || response[0] != reg) {
		return false;
	}

	value.year = static_cast<std::uint16_t>(2000U + response[1]);
	value.month = response[2];
	value.day = response[3];
	value.weekday = response[4];
	return lexxos::devices::rtc::isValidDate(value);
}

bool PicoCalcSouthbridge::_readJcsRtcTime(std::uint8_t reg, lexxos::devices::rtc::DateTime& value) const {
	std::uint8_t response[4]{};
	if (!_readBytes(reg, response, 4) || response[0] != reg) {
		return false;
	}

	value.hour = response[1];
	value.minute = response[2];
	value.second = response[3];
	return lexxos::devices::rtc::isValidDate(value);
}

bool PicoCalcSouthbridge::_writeJcsRtcDate(
    std::uint8_t reg,
    lexxos::devices::rtc::DateTime const& value,
    PicoCalcSouthbridgeCommandState* state) const {
	if (!lexxos::devices::rtc::isValidDate(value) || value.year < 2000U || value.year > 2099U) {
		setWriteState(state, PicoCalcSouthbridgeCommandState::Refused);
		return false;
	}

	std::uint8_t const data[3]{
	    static_cast<std::uint8_t>(value.year - 2000U),
	    value.month,
	    value.day,
	};
	std::uint8_t response[5]{};
	if (!_writeBytes(reg, data, 3, response, 5, state)) {
		return false;
	}
	if (response[1] != data[0] || response[2] != data[1] || response[3] != data[2]) {
		setWriteState(state, PicoCalcSouthbridgeCommandState::Refused);
		return false;
	}
	return true;
}

bool PicoCalcSouthbridge::_writeJcsRtcTime(
    std::uint8_t reg,
    lexxos::devices::rtc::DateTime const& value,
    PicoCalcSouthbridgeCommandState* state) const {
	if (!lexxos::devices::rtc::isValidDate(value)) {
		setWriteState(state, PicoCalcSouthbridgeCommandState::Refused);
		return false;
	}

	std::uint8_t const data[3]{
	    value.hour,
	    value.minute,
	    value.second,
	};
	std::uint8_t response[4]{};
	if (!_writeBytes(reg, data, 3, response, 4, state)) {
		return false;
	}
	if (response[1] != data[0] || response[2] != data[1] || response[3] != data[2]) {
		setWriteState(state, PicoCalcSouthbridgeCommandState::Refused);
		return false;
	}
	return true;
}

} // namespace lexxos::devices
