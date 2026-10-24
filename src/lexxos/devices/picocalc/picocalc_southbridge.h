#pragma once

#include <cstdint>

#include <lexxos/devices/rtc/date_time.h>

namespace lexxos::devices {

class I2cBus;

struct PicoCalcKeyboardState {
	std::uint8_t state{0};
	std::uint8_t value{0};

	[[nodiscard]] std::uint16_t raw() const;
	[[nodiscard]] std::uint8_t fifoCount() const;
	[[nodiscard]] std::uint8_t legacyFifoCount() const;
	[[nodiscard]] bool capsLock() const;
	[[nodiscard]] bool numLock() const;
};

struct PicoCalcKeyboardFifoEvent {
	std::uint8_t status{0};
	std::uint8_t code{0};

	[[nodiscard]] std::uint16_t raw() const;
	[[nodiscard]] bool isEmpty() const;
};

struct PicoCalcBatteryStatus {
	std::uint8_t raw{0};

	[[nodiscard]] bool charging() const;
	[[nodiscard]] std::uint8_t value() const;
};

struct PicoCalcJcsFirmwareInfo {
	std::uint8_t type{0};
	std::uint8_t version{0};

	[[nodiscard]] bool isJcs() const;
	[[nodiscard]] std::uint8_t majorVersion() const;
	[[nodiscard]] std::uint8_t minorVersion() const;
};

struct PicoCalcJcsRtcConfig {
	std::uint8_t raw{0};

	[[nodiscard]] bool alarmEnabled() const;
	[[nodiscard]] bool alarmRearm() const;
};

enum class PicoCalcSouthbridgeCommandKind : std::uint8_t {
	None,
	LcdBacklight,
	KeyboardBacklight,
	Reset,
	PowerOff,
};

enum class PicoCalcSouthbridgeCommandState : std::uint8_t {
	None,
	Running,
	Ok,
	Failed,
	Refused,
};

struct PicoCalcSouthbridgeCommandStatus {
	PicoCalcSouthbridgeCommandKind kind{PicoCalcSouthbridgeCommandKind::None};
	PicoCalcSouthbridgeCommandState state{PicoCalcSouthbridgeCommandState::None};
	std::uint8_t requestedValue{0};
	std::uint8_t acceptedValue{0};
	std::uint32_t requestedMs{0};
	std::uint32_t completedMs{0};
	bool acceptedValueValid{false};
};

struct PicoCalcSouthbridgeSnapshot {
	static constexpr std::uint16_t validVersion = 1U << 0U;
	static constexpr std::uint16_t validConfig = 1U << 1U;
	static constexpr std::uint16_t validInterruptStatus = 1U << 2U;
	static constexpr std::uint16_t validDebounce = 1U << 3U;
	static constexpr std::uint16_t validPollingFrequency = 1U << 4U;
	static constexpr std::uint16_t validKeyboardState = 1U << 5U;
	static constexpr std::uint16_t validBattery = 1U << 6U;
	static constexpr std::uint16_t validLcdBacklight = 1U << 7U;
	static constexpr std::uint16_t validKeyboardBacklight = 1U << 8U;
	static constexpr std::uint16_t validC64Matrix = 1U << 9U;
	static constexpr std::uint16_t validC64Joystick = 1U << 10U;
	static constexpr std::uint16_t validPowerOffSupport = 1U << 11U;
	static constexpr std::uint16_t validFirmwareType = 1U << 12U;
	static constexpr std::uint16_t validJcsRtcConfig = 1U << 13U;
	static constexpr std::uint16_t validJcsRtcDateTime = 1U << 14U;
	static constexpr std::uint16_t validJcsRtcAlarm = 1U << 15U;

	std::uint16_t validMask{0};
	std::uint8_t firmwareType{0};
	std::uint8_t version{0};
	std::uint8_t config{0};
	std::uint8_t interruptStatus{0};
	std::uint8_t debounce{0};
	std::uint8_t pollingFrequency{0};
	PicoCalcKeyboardState keyboardState{};
	PicoCalcBatteryStatus battery{};
	std::uint8_t lcdBacklight{0};
	std::uint8_t keyboardBacklight{0};
	std::uint8_t c64Matrix{0};
	std::uint8_t c64Joystick{0};
	std::uint8_t powerOffSupport{0};
	PicoCalcJcsRtcConfig jcsRtcConfig{};
	lexxos::devices::rtc::DateTime jcsRtcDateTime{};
	lexxos::devices::rtc::DateTime jcsRtcAlarm{};

	[[nodiscard]] bool has(std::uint16_t flag) const;
	[[nodiscard]] bool empty() const;
	[[nodiscard]] std::uint8_t validFieldCount() const;
	void clear();
	void setValid(std::uint16_t flag);
	void setKeyboardState(PicoCalcKeyboardState const& state);
	void setKeyboardState(std::uint8_t state, std::uint8_t value);
};

class PicoCalcSouthbridge final {
public:
	static constexpr std::uint8_t defaultAddress = 0x1F;
	static constexpr std::uint8_t registerFirmwareType = 0x00;
	static constexpr std::uint8_t registerVersion = 0x01;
	static constexpr std::uint8_t registerConfig = 0x02;
	static constexpr std::uint8_t registerInterruptStatus = 0x03;
	static constexpr std::uint8_t registerKeyboardState = 0x04;
	static constexpr std::uint8_t registerLcdBacklight = 0x05;
	static constexpr std::uint8_t registerDebounce = 0x06;
	static constexpr std::uint8_t registerPollingFrequency = 0x07;
	static constexpr std::uint8_t registerReset = 0x08;
	static constexpr std::uint8_t registerKeyboardFifo = 0x09;
	static constexpr std::uint8_t registerKeyboardBacklight = 0x0A;
	static constexpr std::uint8_t registerBattery = 0x0B;
	static constexpr std::uint8_t registerC64Matrix = 0x0C;
	static constexpr std::uint8_t registerC64Joystick = 0x0D;
	static constexpr std::uint8_t registerPowerOff = 0x0E;
	static constexpr std::uint8_t registerJcsInterruptConfig = 0x12;
	static constexpr std::uint8_t registerJcsRtcConfig = 0x13;
	static constexpr std::uint8_t registerJcsRtcDate = 0x14;
	static constexpr std::uint8_t registerJcsRtcTime = 0x15;
	static constexpr std::uint8_t registerJcsRtcAlarmDate = 0x16;
	static constexpr std::uint8_t registerJcsRtcAlarmTime = 0x17;
	static constexpr std::uint8_t jcsFirmwareType = 0xCA;
	static constexpr std::uint8_t keyboardFifoCountMask = 0x1F;
	static constexpr std::uint8_t keyboardCapsLockMask = 0x20;
	static constexpr std::uint8_t keyboardNumLockMask = 0x40;
	static constexpr std::uint8_t batteryChargingMask = 0x80;
	static constexpr std::uint8_t batteryValueMask = 0x7F;
	static constexpr std::uint8_t jcsRtcAlarmEnabledMask = 0x01;
	static constexpr std::uint8_t jcsRtcAlarmRearmMask = 0x02;

	PicoCalcSouthbridge(I2cBus& bus, std::uint8_t address = defaultAddress);

	[[nodiscard]] std::uint8_t address() const;
	[[nodiscard]] bool probe() const;
	[[nodiscard]] bool isReady() const;

	[[nodiscard]] bool readVersion(std::uint8_t& value) const;
	[[nodiscard]] bool readConfig(std::uint8_t& value) const;
	[[nodiscard]] bool setConfig(std::uint8_t value, std::uint8_t* acceptedValue = nullptr, PicoCalcSouthbridgeCommandState* state = nullptr) const;
	[[nodiscard]] bool readInterruptStatus(std::uint8_t& value) const;
	[[nodiscard]] bool readDebounce(std::uint8_t& value) const;
	[[nodiscard]] bool setDebounce(std::uint8_t value, std::uint8_t* acceptedValue = nullptr, PicoCalcSouthbridgeCommandState* state = nullptr) const;
	[[nodiscard]] bool readPollingFrequency(std::uint8_t& value) const;
	[[nodiscard]] bool setPollingFrequency(std::uint8_t value, std::uint8_t* acceptedValue = nullptr, PicoCalcSouthbridgeCommandState* state = nullptr) const;
	[[nodiscard]] bool readKeyboardState(PicoCalcKeyboardState& state) const;
	[[nodiscard]] bool readKeyboardFifo(PicoCalcKeyboardFifoEvent& event) const;
	[[nodiscard]] bool readBattery(PicoCalcBatteryStatus& status) const;
	[[nodiscard]] bool readLcdBacklight(std::uint8_t& value) const;
	[[nodiscard]] bool setLcdBacklight(std::uint8_t value, std::uint8_t* acceptedValue = nullptr, PicoCalcSouthbridgeCommandState* state = nullptr) const;
	[[nodiscard]] bool readKeyboardBacklight(std::uint8_t& value) const;
	[[nodiscard]] bool setKeyboardBacklight(std::uint8_t value, std::uint8_t* acceptedValue = nullptr, PicoCalcSouthbridgeCommandState* state = nullptr) const;
	[[nodiscard]] bool readC64Matrix(std::uint8_t& value) const;
	[[nodiscard]] bool readC64Joystick(std::uint8_t& value) const;
	[[nodiscard]] bool readPowerOffSupport(std::uint8_t& value) const;
	[[nodiscard]] bool readFirmwareType(std::uint8_t& value) const;
	[[nodiscard]] bool readJcsFirmwareInfo(PicoCalcJcsFirmwareInfo& info) const;
	[[nodiscard]] bool readJcsRtcConfig(PicoCalcJcsRtcConfig& config) const;
	[[nodiscard]] bool setJcsRtcConfig(
	    PicoCalcJcsRtcConfig config,
	    PicoCalcJcsRtcConfig* acceptedConfig = nullptr,
	    PicoCalcSouthbridgeCommandState* state = nullptr) const;
	[[nodiscard]] bool readJcsRtcDateTime(lexxos::devices::rtc::DateTime& value) const;
	[[nodiscard]] bool setJcsRtcDateTime(
	    lexxos::devices::rtc::DateTime const& value,
	    PicoCalcSouthbridgeCommandState* state = nullptr) const;
	[[nodiscard]] bool readJcsRtcAlarm(lexxos::devices::rtc::DateTime& value) const;
	[[nodiscard]] bool setJcsRtcAlarm(
	    lexxos::devices::rtc::DateTime const& value,
	    PicoCalcSouthbridgeCommandState* state = nullptr) const;
	[[nodiscard]] bool requestReset(std::uint8_t delaySeconds, PicoCalcSouthbridgeCommandState* state = nullptr) const;
	[[nodiscard]] bool requestPowerOff(PicoCalcSouthbridgeCommandState* state = nullptr) const;
	[[nodiscard]] bool readSnapshot(PicoCalcSouthbridgeSnapshot& snapshot) const;

private:
	static constexpr std::uint8_t registerWriteMask = 0x80;
	static constexpr std::uint8_t writeResponseRetryCount = 3;
	static constexpr std::uint8_t jcsResetPicoMask = 0x40;
	static constexpr std::uint8_t jcsPowerSleepMask = 0x40;
	[[nodiscard]] bool _readPair(std::uint8_t reg, std::uint8_t (&response)[2]) const;
	[[nodiscard]] bool _readBytes(std::uint8_t reg, std::uint8_t* response, std::uint8_t responseSize) const;
	[[nodiscard]] bool _readValue(std::uint8_t reg, std::uint8_t& value) const;
	[[nodiscard]] bool _writeBytes(
	    std::uint8_t reg,
	    std::uint8_t const* data,
	    std::uint8_t size,
	    std::uint8_t* response,
	    std::uint8_t responseSize,
	    PicoCalcSouthbridgeCommandState* state) const;
	[[nodiscard]] bool _writeValue(std::uint8_t reg, std::uint8_t value, std::uint8_t* acceptedValue, PicoCalcSouthbridgeCommandState* state) const;
	[[nodiscard]] bool _readJcsRtcDate(std::uint8_t reg, lexxos::devices::rtc::DateTime& value) const;
	[[nodiscard]] bool _readJcsRtcTime(std::uint8_t reg, lexxos::devices::rtc::DateTime& value) const;
	[[nodiscard]] bool _writeJcsRtcDate(
	    std::uint8_t reg,
	    lexxos::devices::rtc::DateTime const& value,
	    PicoCalcSouthbridgeCommandState* state) const;
	[[nodiscard]] bool _writeJcsRtcTime(
	    std::uint8_t reg,
	    lexxos::devices::rtc::DateTime const& value,
	    PicoCalcSouthbridgeCommandState* state) const;

	I2cBus& _bus;
	std::uint8_t _address{defaultAddress};
};

} // namespace lexxos::devices
