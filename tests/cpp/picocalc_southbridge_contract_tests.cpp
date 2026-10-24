#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/picocalc/picocalc_southbridge.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <initializer_list>
#include <iostream>
#include <utility>
#include <vector>

namespace {

using lexxos::devices::PicoCalcSouthbridge;
using lexxos::devices::PicoCalcSouthbridgeCommandState;
using lexxos::devices::rtc::DateTime;

void expect(bool condition, char const* message) {
	if (!condition) {
		std::cerr << "picocalc southbridge contract test failed: " << message << '\n';
		std::exit(1);
	}
}

class ScriptedI2cBus final : public lexxos::devices::I2cBus {
public:
	void queueRead(std::vector<std::uint8_t> response) {
		_reads.push_back(std::move(response));
	}

	bool initialize(lexxos::platform::I2cPins const&) override {
		_ready = true;
		return true;
	}

	[[nodiscard]] bool isReady() const override {
		return _ready;
	}

	bool probe(std::uint8_t) override {
		return true;
	}

	bool write(std::uint8_t address, std::uint8_t const* data, std::size_t size) override {
		_lastAddress = address;
		_writes.emplace_back(data, data + size);
		return true;
	}

	bool read(std::uint8_t address, std::uint8_t* data, std::size_t size) override {
		expect(address == _lastAddress, "read address follows last write address");
		if (_reads.empty() || _reads.front().size() != size) {
			return false;
		}
		auto const response = _reads.front();
		_reads.pop_front();
		std::memcpy(data, response.data(), size);
		return true;
	}

	[[nodiscard]] std::vector<std::uint8_t> const& writeAt(std::size_t index) const {
		return _writes[index];
	}

	[[nodiscard]] std::size_t writeCount() const {
		return _writes.size();
	}

	[[nodiscard]] std::size_t queuedReadCount() const {
		return _reads.size();
	}

private:
	bool _ready{true};
	std::uint8_t _lastAddress{0};
	std::deque<std::vector<std::uint8_t>> _reads{};
	std::vector<std::vector<std::uint8_t>> _writes{};
};

DateTime sampleDateTime(std::uint8_t second = 29) {
	return {
	    .year = 2026,
	    .month = 7,
	    .day = 2,
	    .weekday = 4,
	    .hour = 8,
	    .minute = 29,
	    .second = second,
	};
}

void queueDateWriteEcho(ScriptedI2cBus& bus) {
	bus.queueRead({PicoCalcSouthbridge::registerJcsRtcDate, 26, 7, 2, 4});
}

void queueTimeWriteEcho(ScriptedI2cBus& bus, std::uint8_t second) {
	bus.queueRead({PicoCalcSouthbridge::registerJcsRtcTime, 8, 29, second});
}

void queueDateReadback(ScriptedI2cBus& bus) {
	bus.queueRead({PicoCalcSouthbridge::registerJcsRtcDate, 26, 7, 2, 4});
}

void queueTimeReadback(ScriptedI2cBus& bus, std::uint8_t second) {
	bus.queueRead({PicoCalcSouthbridge::registerJcsRtcTime, 8, 29, second});
}

void expectWrite(std::vector<std::uint8_t> const& actual, std::initializer_list<std::uint8_t> expected, char const* message) {
	expect(actual.size() == expected.size(), message);
	expect(std::equal(actual.begin(), actual.end(), expected.begin()), message);
}

void checkJcsRtcWriteAcceptsExactEcho() {
	ScriptedI2cBus bus;
	PicoCalcSouthbridge southbridge{bus};
	queueDateWriteEcho(bus);
	queueTimeWriteEcho(bus, 29);

	PicoCalcSouthbridgeCommandState state = PicoCalcSouthbridgeCommandState::Failed;
	expect(southbridge.setJcsRtcDateTime(sampleDateTime(), &state), "exact RTC write echo succeeds");
	expect(state == PicoCalcSouthbridgeCommandState::Ok, "exact RTC write state is ok");
	expect(bus.queuedReadCount() == 0, "exact RTC write consumes scripted reads");
	expect(bus.writeCount() == 2, "exact RTC write uses date and time writes");
	expectWrite(bus.writeAt(0), {0x94, 26, 7, 2}, "date write command bytes");
	expectWrite(bus.writeAt(1), {0x95, 8, 29, 29}, "time write command bytes");
}

void checkJcsRtcWriteAcceptsTickingReadback() {
	ScriptedI2cBus bus;
	PicoCalcSouthbridge southbridge{bus};
	queueDateWriteEcho(bus);
	queueTimeWriteEcho(bus, 30);
	queueDateReadback(bus);
	queueTimeReadback(bus, 30);

	PicoCalcSouthbridgeCommandState state = PicoCalcSouthbridgeCommandState::Failed;
	expect(southbridge.setJcsRtcDateTime(sampleDateTime(), &state), "RTC write accepts a one-second readback tick");
	expect(state == PicoCalcSouthbridgeCommandState::Ok, "ticking RTC write state is ok after readback");
	expect(bus.queuedReadCount() == 0, "ticking RTC write consumes scripted reads");
	expect(bus.writeCount() == 4, "ticking RTC write verifies readback after mismatched echo");
	expectWrite(bus.writeAt(2), {PicoCalcSouthbridge::registerJcsRtcDate}, "date readback register write");
	expectWrite(bus.writeAt(3), {PicoCalcSouthbridge::registerJcsRtcTime}, "time readback register write");
}

void checkJcsRtcWriteRejectsWrongReadback() {
	ScriptedI2cBus bus;
	PicoCalcSouthbridge southbridge{bus};
	queueDateWriteEcho(bus);
	queueTimeWriteEcho(bus, 40);
	queueDateReadback(bus);
	queueTimeReadback(bus, 40);

	PicoCalcSouthbridgeCommandState state = PicoCalcSouthbridgeCommandState::Failed;
	expect(!southbridge.setJcsRtcDateTime(sampleDateTime(), &state), "RTC write rejects far readback time");
	expect(state == PicoCalcSouthbridgeCommandState::Refused, "rejected RTC write keeps refused state");
	expect(bus.queuedReadCount() == 0, "rejected RTC write consumes scripted reads");
}

} // namespace

void checkPicoCalcSouthbridgeRtcWrite() {
	checkJcsRtcWriteAcceptsExactEcho();
	checkJcsRtcWriteAcceptsTickingReadback();
	checkJcsRtcWriteRejectsWrongReadback();
}
