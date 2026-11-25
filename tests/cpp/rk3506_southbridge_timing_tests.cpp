#include <cstdlib>
#include <iostream>

#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/devices/picocalc/picocalc_southbridge.h>

namespace {

std::uint64_t counter;
std::uint32_t frequency;

void expect(bool condition, char const* message) {
	if (!condition) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}

class TimedBus final : public lexxos::devices::I2cBus {
public:
	bool failWrite{false};
	bool active{false};
	unsigned reads{0};
	std::uint64_t writtenAt{};

	bool initialize(lexxos::platform::I2cPins const&) override { return true; }

	bool isReady() const override { return true; }

	bool probe(std::uint8_t) override { return true; }

	bool beginTransaction() override {
		active = true;
		return true;
	}

	void endTransaction() override { active = false; }

	bool write(std::uint8_t, std::uint8_t const*, std::size_t) override {
		expect(active, "register selection must be inside a transaction");
		writtenAt = counter;
		return !failWrite;
	}

	bool read(std::uint8_t, std::uint8_t* data, std::size_t size) override {
		expect(active, "response must stay inside the same transaction");
		expect(counter - writtenAt >= (static_cast<std::uint64_t>(frequency) * 2U + 999U) / 1000U,
		    "RK3506 must wait at least 2 ms before reading STM32 response");
		++reads;
		for (std::size_t i = 0; i < size; ++i) {
			data[i] = 0;
		}

		return true;
	}
};

} // namespace

extern "C" std::uint64_t rk3506_timer_counter() {
	return counter++;
}

extern "C" std::uint32_t rk3506_timer_counter_frequency() {
	return frequency;
}

int main() {
	for (auto hz : {24000000U, 32768U}) {
		frequency = hz;
		TimedBus bus;
		lexxos::devices::PicoCalcSouthbridge southbridge{bus};
		lexxos::devices::PicoCalcKeyboardState state{};
		expect(southbridge.readKeyboardState(state), "keyboard state read succeeds");
		expect(bus.reads == 1 && !bus.active, "transaction is released after success");
		bus.failWrite = true;
		expect(!southbridge.readKeyboardState(state), "failed register write propagates");
		expect(bus.reads == 1 && !bus.active, "failed write skips read and releases transaction");
	}
}
