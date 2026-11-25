#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>

#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/bus/i2c_transaction_queue.h>
#include <lexxos/devices/device_manager.h>

namespace {

class TestI2cBus final : public lexxos::devices::I2cBus {
public:
	bool initialize(lexxos::platform::I2cPins const&) override {
		return true;
	}

	bool isReady() const override { return true; }

	bool probe(std::uint8_t) override { return true; }

	bool write(std::uint8_t, std::uint8_t const*,
	    std::size_t) override { return true; }

	bool read(std::uint8_t, std::uint8_t*,
	    std::size_t) override { return true; }
};

} // namespace

int main() {
	using lexxos::devices::DeviceManager;
	using lexxos::devices::DeviceResourcePriority;
	using lexxos::devices::I2cTransactionQueue;

	// A higher-priority request queued later must run before Background.
	I2cTransactionQueue queue;
	queue.setWaitFunction([](std::uint32_t milliseconds) {
		std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
	});
	assert(queue.acquire(DeviceResourcePriority::Normal, 0));
	std::atomic_int order{0};
	std::atomic_int systemOrder{0};
	std::atomic_int backgroundOrder{0};
	std::thread background([&] {
		if (queue.acquire(DeviceResourcePriority::Background, 3000)) {
			backgroundOrder = ++order;
			queue.release();
		}
	});
	for (int i = 0; queue.pending() < 1 && i < 1000; ++i) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	std::thread system([&] {
		if (queue.acquire(DeviceResourcePriority::System, 3000)) {
			systemOrder = ++order;
			queue.release();
		}
	});
	for (int i = 0; queue.pending() < 2 && i < 1000; ++i) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	assert(queue.pending() == 2);
	queue.release();
	system.join();
	background.join();
	assert(systemOrder == 1);
	assert(backgroundOrder == 2);
	assert(!queue.busy());
	assert(queue.pending() == 0);

	DeviceManager devices;
	devices.initialize();
	TestI2cBus bus;

	assert(devices.registerI2cBus("debug", "Debug I2C", bus));
	assert(devices.registerI2cBus("system", "System I2C", bus));
	assert(devices.reserveI2cAddress("system", 0x1F, "keyboard"));
	assert(devices.isI2cAddressReserved(bus, 0x1F));
	assert(!devices.isI2cAddressReserved(bus, 0x68));
	assert(!devices.reserveI2cAddress("debug", 0x1F, "guest"));
	assert(devices.reserveI2cAddress("debug", 0x1F, "keyboard"));
	assert(devices.i2cBusCount() == 2);

	{
		auto first = devices.lockI2cBus(
		    "debug", DeviceResourcePriority::System, "system");
		assert(first);
		assert(devices.isI2cBusLocked("debug"));
		assert(devices.isI2cBusLocked("system"));

		auto second = devices.lockI2cBus(
		    "system", DeviceResourcePriority::Normal, "guest");
		assert(!second);

		assert(devices.i2cBusStatusAt(0).locked);
		assert(devices.i2cBusStatusAt(1).locked);
	}

	auto released = devices.lockI2cBus(
	    "system", DeviceResourcePriority::Normal, "guest");
	assert(released);
	released.release();

	assert(devices.registerGpioPin(
	    "test-output", "Test GPIO", {12}, false, true));
	assert(!devices.registerGpioPin(
	    "duplicate", "Same physical pin", {12}, true, true));

	{
		auto pin = devices.lockGpioPin("test-output");
		assert(pin);
		assert(!devices.lockGpioPin("test-output"));
		assert(devices.gpioPinStatusAt(0).locked);
	}

	assert(!devices.gpioPinStatusAt(0).locked);

	return 0;
}
