#include <cstdlib>
#include <iostream>

#include <lexxos/composition/board_device_providers.h>
#include <lexxos/composition/detail/platform_device_providers.h>
#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/device_manager.h>
#include <lexxos/devices/sd_card.h>
#include <lexxos/platform/platform_services.h>

namespace {

using lexxos::composition::BoardDeviceProvider;
using lexxos::devices::DeviceManager;
using lexxos::platform::LegacyBoardDescriptor;
using lexxos::platform::PlatformServices;

void expect(bool condition, char const* message) {
	if (!condition) {
		std::cerr << "board providers: " << message << '\n';
		std::exit(1);
	}
}

class ProviderTestBus final : public lexxos::devices::I2cBus {
public:
	unsigned initializations{};

	bool initialize(lexxos::platform::I2cPins const&) override {
		++initializations;
		return true;
	}

	bool isReady() const override { return initializations != 0; }

	bool probe(std::uint8_t) override { return false; }

	bool write(std::uint8_t, std::uint8_t const*, std::size_t) override { return false; }
};

ProviderTestBus debugBus, systemBus, nativeBus;
LegacyBoardDescriptor board{};
PlatformServices const* expectedServices{};
unsigned calls{};

LegacyBoardDescriptor const& currentBoard() {
	return board;
}

void first(DeviceManager& devices, PlatformServices const& services, LegacyBoardDescriptor const& activeBoard) {
	expect(calls++ == 0, "first provider order");
	expect(&services == expectedServices && &activeBoard == &board, "injected services and board");
	expect(devices.i2cBusCount() == 0, "custom list must not implicitly add legacy buses");
	expect(devices.registerI2cBus("native", "test native bus", nativeBus), "register native bus");
}

void second(DeviceManager& devices, PlatformServices const&, LegacyBoardDescriptor const&) {
	expect(calls++ == 1, "second provider order");
	expect(devices.i2cBus("native") == &nativeBus, "prior provider registration visible");
}

} // namespace

namespace lexxos::devices {

I2cBus& debugI2cBus() {
	return debugBus;
}

I2cBus& systemI2cBus() {
	return systemBus;
}

namespace sd_card {

	BlockDevice* blockDevice(platform::PlatformServices const& services) {
		expect(&services == expectedServices, "SD receives injected services");
		return nullptr;
	}

} // namespace lexxos::devices::sd_card

} // namespace lexxos::devices

void checkBoardDeviceProviders() {
	PlatformServices services{};
	services.board.current = currentBoard;
	expectedServices = &services;
	BoardDeviceProvider const providers[]{first, second};
	DeviceManager native;
	expect(lexxos::composition::registerBoardDevices(native, services, providers), "custom composition");
	expect(calls == 2 && native.i2cBusCount() == 1, "exact provider list");

	DeviceManager invalid;
	calls = 0;
	BoardDeviceProvider const invalidProviders[]{first, nullptr};
	expect(!lexxos::composition::registerBoardDevices(invalid, services, invalidProviders), "null provider rejected");
	expect(calls == 0 && invalid.i2cBusCount() == 0, "reject entire list before effects");

	board.userGpio.count = lexxos::platform::maxBoardUserGpioPins + 1;
	expect(!lexxos::composition::registerBoardDevices(invalid, services, providers), "invalid GPIO rejected");
	expect(calls == 0, "GPIO validation precedes providers");
	board.userGpio.count = 0;
	expect(lexxos::composition::registerBoardDevices(invalid, services, {}), "empty composition");
	expect(invalid.i2cBusCount() == 0, "empty list adds no buses");

	// Same physical pins share one bus even when two names are published.
	board.debugOledI2c = {.index = 0, .sda = {2}, .scl = {3}, .baudRate = 100000};
	board.interfaces.systemI2c = board.debugOledI2c;
	DeviceManager legacy;
	expect(lexxos::composition::registerBoardDevices(legacy, services), "selected host composition");
	expect(legacy.i2cBus("debug") == &debugBus && legacy.i2cBus("system") == &debugBus, "physical bus aliases");
	expect(debugBus.initializations == 1 && systemBus.initializations == 0, "initialize aliased bus once");

	board.interfaces.systemI2c.index = 1;
	DeviceManager separate;
	expect(lexxos::composition::registerBoardDevices(separate, services), "separate physical buses");
	expect(separate.i2cBus("debug") == &debugBus && separate.i2cBus("system") == &systemBus, "distinct bus wiring");
	expect(debugBus.initializations == 1 && systemBus.initializations == 1, "ready bus not initialized twice");
	board.debugOledI2c = {};
	board.interfaces.systemI2c = {};
}
