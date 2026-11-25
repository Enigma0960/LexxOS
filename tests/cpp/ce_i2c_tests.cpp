#include <cstdlib>
#include <iostream>
#include <string>

#include <board.h>

#include <lexxos/devices/device_manager.h>

extern "C" void app_main();

namespace {

using lexxos::devices::I2cBus;
using lexxos::platform::LegacyBoardDescriptor;
using lexxos::platform::NamedI2cBus;
std::string logText;
int live{};
int created{};
unsigned nextAddress{};
bool respond = true;
bool failSecond{};

void expect(bool ok, char const* message) {
	if (!ok) {
		std::cerr << message << '\n'
		          << logText;
		std::exit(1);
	}
}

class FakeBus final : public I2cBus {
public:
	FakeBus() {
		++live;
		++created;
	}

	~FakeBus() override { --live; }

	bool initialize(lexxos::platform::I2cPins const& pins) override {
		return !(failSecond && pins.index == 1);
	}

	bool isReady() const override { return true; }

	bool probe(std::uint8_t address) override {
		expect(address == nextAddress++, "scan range");
		return respond && (address == 0x4A || address == 0x76 || address == 0x77);
	}

	bool write(std::uint8_t, std::uint8_t const*, std::size_t) override {
		expect(false, "separate write during register read");
		return false;
	}

	bool writeRead(std::uint8_t address, std::uint8_t const* tx, std::size_t txSize,
	    std::uint8_t* rx, std::size_t rxSize) override {
		expect(tx && txSize == 1 && *tx == 0xD0 && rx && rxSize == 1, "register exchange");
		expect(address == 0x76 || address == 0x77, "unexpected register access");
		*rx = address == 0x77 ? 0x60 : 0x58;
		return true;
	}
};

constexpr NamedI2cBus buses[]{
    {"sensors", {.index = 0, .sda = {21}, .scl = {22}, .baudRate = 100000}},
    {    "aux", {.index = 1, .sda = {25}, .scl = {26}, .baudRate = 100000}},
};

LegacyBoardDescriptor const& testBoard() {
	static constexpr LegacyBoardDescriptor board{.i2cBuses = buses};
	return board;
}

} // namespace

lexxos::platform::PlatformServices const& i2c_sensor::platform() {
	static lexxos::platform::PlatformServices const services{
	    .board = {.current = testBoard},
	    .console = {.write = [](std::string_view line) { logText += line; }},
	    .createI2cBus = []() -> std::unique_ptr<I2cBus> { return std::make_unique<FakeBus>(); },
	};
	return services;
}

namespace lexxos::platform {

LegacyBoardDescriptor const& board() {
	return testBoard();
}

void sleepMs(std::uint32_t) {
	expect(false, "unexpected sleep");
}

void writeConsole(std::string_view) {
	expect(false, "unexpected fallback console");
}

} // namespace lexxos::platform

int main() {
	lexxos::devices::DeviceManager devices;
	auto const& platform = i2c_sensor::platform();
	auto board = testBoard();
	board.statusLed = {22};
	expect(!devices.bindBoardI2c(board, platform) && created == 0, "shared GPIO accepted");
	board = testBoard();
	failSecond = true;
	expect(!devices.bindBoardI2c(board, platform) && live == 0 && devices.i2cBusCount() == 0, "partial registration");
	failSecond = false;
	expect(devices.bindBoardI2c(board, platform), "retry after failure");
	auto* bus = devices.i2cBus("sensors");
	expect(bus && bus == devices.i2cBus("sensors") && devices.i2cBus("aux") != bus, "bus identity");
	devices.initialize();
	expect(live == 0, "bus leak after reset");

	nextAddress = 8;
	app_main();
	expect(nextAddress == 0x78 && logText.find("scan complete: 3") != std::string::npos, "scan results");
	expect(logText.find("ACK 0x4A") != std::string::npos, "missing second sensor");
	expect(logText.find("0x77 chip_id=0x60 BME280") != std::string::npos, "BME280 identification");
	expect(logText.find("0x76 chip_id=0x58\r\n") != std::string::npos, "unexpected device labelled BME280");

	respond = false;
	logText.clear();
	nextAddress = 8;
	app_main();
	expect(nextAddress == 0x78 && logText.find("scan complete: 0") != std::string::npos, "empty bus");
}
