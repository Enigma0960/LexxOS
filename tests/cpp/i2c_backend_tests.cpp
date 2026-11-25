#include <array>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <vector>

#include <lexxos/devices/bus/software_i2c_bus.h>
#include <lexxos/devices/rk3506/rk3506_i2c_bus.h>

namespace {

void expect(bool ok, char const* message) {
	if (!ok) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}

struct Hardware {
	std::array<std::uint32_t, 160> regs{};
	std::vector<std::uint8_t> sent;
	std::vector<std::uint32_t> receiveControls;
	std::uint64_t tick{};
	std::size_t received{};
	unsigned starts{}, stops{};
	std::uint32_t stalledEvent{};
	bool nak{}, preemptBeforePoll{}, recoveryFails{};
	unsigned recoveries{};
	std::uint32_t completeAtDeadline{};

	lexxos::devices::Rk3506I2cIo io() {
		return {this,
		    [](void*, lexxos::platform::I2cPins const&) { return true; },
		    [](void* p, std::uint32_t offset) {
			    auto& hw = *static_cast<Hardware*>(p);
			    if (offset == 0x1C && hw.preemptBeforePoll) {
				    hw.tick += 1000; // device completed while this task was preempted
				    hw.preemptBeforePoll = false;
			    }

			    return hw.regs[offset / 4];
		    },
		    [](void* p, std::uint32_t offset, std::uint32_t value) { static_cast<Hardware*>(p)->write(offset, value); },
		    [](void* p) {
			    auto& hw = *static_cast<Hardware*>(p);
			    if (hw.completeAtDeadline && hw.tick >= hw.completeAtDeadline) {
				    hw.regs[0x1C / 4] |= hw.stalledEvent;
				    hw.stalledEvent = 0;
				    hw.completeAtDeadline = 0;
			    }

			    return hw.tick++;
		    },
		    [](void*) -> std::uint32_t { return 1000; },
		    [](void* p, lexxos::platform::I2cPins const&) {
			    auto& hw = *static_cast<Hardware*>(p);
			    ++hw.recoveries;
			    if (hw.recoveryFails) {
				    return false;
			    }

			    hw.regs.fill(0); // peripheral reset also loses CLKDIV
			    hw.stalledEvent = 0;
			    return true;
		    }};
	}

	void event(std::uint32_t bits) {
		expect((regs[0x18 / 4] & bits) != 0, "controller event must be enabled before polling");
		if (bits != stalledEvent) {
			regs[0x1C / 4] |= bits;
		}
	}

	void write(std::uint32_t offset, std::uint32_t value) {
		if (offset == 0x1C) {
			regs[offset / 4] &= ~value;
			return;
		}

		regs[offset / 4] = value;
		if (offset == 0 && (value & 8)) {
			++starts;
			event(16);
		}

		if (offset == 0 && (value & 16)) {
			++stops;
			event(32);
		}

		if (offset == 0x10) {
			expect(value <= 32, "TX exceeds FIFO");
			for (unsigned i = 0; i < value; ++i) {
				sent.push_back(static_cast<std::uint8_t>(regs[0x100 / 4 + i / 4] >> (8 * (i % 4))));
			}

			event(4);
			if (nak) {
				regs[0x1C / 4] |= 64;
			}
		}

		if (offset == 0x14) {
			expect(value <= 32, "RX exceeds FIFO");
			receiveControls.push_back(regs[0]);
			for (unsigned i = 0; i < 8; ++i) {
				regs[0x200 / 4 + i] = 0;
			}

			for (unsigned i = 0; i < value; ++i) {
				regs[0x200 / 4 + i / 4] |= static_cast<std::uint32_t>((received++ * 7U) & 255U) << (8 * (i % 4));
			}

			event(8);
			if (nak) {
				regs[0x1C / 4] |= 64;
			}
		}
	}
};

void hardwareTests() {
	lexxos::platform::I2cPins pins{.index = 2, .sda = {11}, .scl = {10}, .baudRate = 100000};
	for (unsigned size : {1U, 2U, 31U, 32U, 33U, 64U, 65U}) {
		Hardware hw;
		lexxos::devices::Rk3506I2cBus bus{hw.io()};
		expect(bus.initialize(pins), "hardware initialization");
		std::vector<std::uint8_t> input(size), output(size);
		for (unsigned i = 0; i < size; ++i) {
			input[i] = static_cast<std::uint8_t>(i * 3U);
		}

		expect(bus.write(0x1F, input.data(), size), "FIFO write");
		expect(hw.sent.size() == size + 1 && hw.sent[0] == 0x3E, "one write address only");
		for (unsigned i = 0; i < size; ++i) {
			expect(hw.sent[i + 1] == input[i], "TX order across FIFO boundary");
		}

		expect(bus.read(0x1F, output.data(), size), "FIFO read");
		for (unsigned i = 0; i < size; ++i) {
			expect(output[i] == static_cast<std::uint8_t>(i * 7U), "RX order across FIFO boundary");
		}

		expect(hw.regs[2] == 0x0100003F && hw.regs[3] == 0, "raw read address without register prefix");
		for (unsigned i = 0; i < hw.receiveControls.size(); ++i) {
			auto control = hw.receiveControls[i];
			expect((control & 6) == (i == 0 ? 2U : 4U), "RX continuation must not resend address");
			expect(bool(control & 32) == (i + 1 == hw.receiveControls.size()), "NACK only final receive chunk");
		}

		expect(hw.starts == 2 && hw.stops == 2, "exactly one START/STOP per operation");
		expect(hw.regs[0] == 0 && hw.regs[6] == 0 && hw.regs[7] == 0, "controller idle and IRQ disabled");
	}
	for (auto stalled : {16U, 4U, 8U, 32U}) {
		Hardware hw;
		lexxos::devices::Rk3506I2cBus bus{hw.io()};
		expect(bus.initialize(pins), "init before timeout");
		hw.stalledEvent = stalled;
		std::uint8_t byte{};
		bool ok = stalled == 8 ? bus.read(0x1F, &byte, 1) : bus.write(0x1F, &byte, 1);

		expect(!ok && hw.tick < 60, "bounded START/data/STOP timeout");
		expect(bus.lastFailure().available && !bus.lastFailure().nack && bus.lastFailure().event == stalled, "failed phase lost during STOP cleanup");
		expect(hw.regs[0] == 0 && hw.regs[6] == 0 && hw.regs[7] == 0, "cleanup after timeout");
		hw.stalledEvent = 0;

		expect(bus.probe(0x1F), "next transaction succeeds after timeout");
		expect(hw.recoveries == 1 && hw.regs[1] == 0x000E000EU, "timeout recovery must restore clock divider");
		expect(bus.lastFailure().recoveryAttempts == 1 && bus.lastFailure().recoverySuccesses == 1, "recovery accounting");
		expect(!bus.lastFailure().available, "new transaction kept stale failure");
	}

	{
		Hardware delayed;
		lexxos::devices::Rk3506I2cBus bus{delayed.io()};
		expect(bus.initialize(pins), "init preemption test");
		delayed.preemptBeforePoll = true;
		expect(bus.probe(0x1F), "completed event must win over elapsed wall-clock deadline");
		expect(!bus.lastFailure().available && delayed.recoveries == 0, "preemption caused false recovery");
	}

	{
		Hardware stuck;
		lexxos::devices::Rk3506I2cBus bus{stuck.io()};
		expect(bus.initialize(pins), "init failed recovery test");
		stuck.stalledEvent = 16;
		expect(!bus.probe(0x1F), "START timeout expected");
		auto const starts = stuck.starts;
		stuck.recoveryFails = true;
		expect(!bus.probe(0x1F) && stuck.starts == starts, "failed bus clear must refuse new transfers");
		expect(bus.lastFailure().available && bus.lastFailure().event == 16, "failed clear erased original cause");
		for (unsigned retry = 0; retry < 10; ++retry) {
			expect(!bus.probe(0x1F), "cooldown must keep refusing transactions");
		}

		expect(stuck.recoveries == 1 && stuck.starts == starts, "retry loop flooded GPIO recovery");
		stuck.tick += 250;
		stuck.recoveryFails = false;
		expect(bus.probe(0x1F) && stuck.recoveries == 2, "next call retries recovery");
	}

	{
		Hardware deadline;
		lexxos::devices::Rk3506I2cBus bus{deadline.io()};
		expect(bus.initialize(pins), "deadline test init");
		deadline.stalledEvent = 4;
		deadline.completeAtDeadline = 21;
		expect(bus.probe(0x1F), "completion at deadline must be consumed before cleanup");
		expect(!bus.lastFailure().available && deadline.recoveries == 0, "spurious timeout at deadline");
	}

	Hardware hw;
	lexxos::devices::Rk3506I2cBus bus{hw.io()};
	expect(bus.initialize(pins), "init before NACK");

	hw.nak = true;
	std::uint8_t byte{};
	expect(!bus.probe(0x1F) && !bus.read(0x1F, &byte, 1), "NACK wins over simultaneous completion");
	expect(bus.lastFailure().available && bus.lastFailure().nack && (bus.lastFailure().pending & 64U), "NACK diagnostic missing");
	hw.nak = false;

	expect(bus.probe(0x1F), "probe recovers after NACK");
	expect(hw.recoveries == 0, "ordinary NACK must not pulse or reset the bus");
	expect(!bus.probe(0x80) && !bus.write(0x1F, nullptr, 1) && !bus.read(0x1F, &byte, 0), "invalid transfer rejected");
}

struct Software {
	std::uint64_t tick{};
	bool sdaReleased{true}, sclReleased{true}, stuckClock{}, stuckData{};
	std::deque<bool> samples;
	std::vector<bool> clockData;
	unsigned configured{}, stretchReads{};

	lexxos::devices::SoftwareI2cIo io() {
		return {this,
		    [](void* p, lexxos::platform::I2cPins const&) { ++static_cast<Software*>(p)->configured; return true; },
		    [](void* p, lexxos::platform::GpioPin pin, bool release) {
			    auto& s = *static_cast<Software*>(p);
			    if (pin.number == 21) {
				    s.sdaReleased = release;
			    } else {
				    if (release && !s.sclReleased) {
					    s.clockData.push_back(s.sdaReleased);
				    }

				    s.sclReleased = release;
			    }
		    },
		    [](void* p, lexxos::platform::GpioPin pin) {
			    auto& s = *static_cast<Software*>(p);
			    if (pin.number == 22) {
				    if (s.stretchReads) {
					    --s.stretchReads;
					    return false;
				    }

				    return !s.stuckClock;
			    }

			    if (s.stuckData) {
				    return false;
			    }

			    expect(!s.samples.empty(), "unexpected SDA sample");
			    bool const value = s.samples.front();
			    s.samples.pop_front();
			    return value;
		    },
		    [](void* p) { return static_cast<Software*>(p)->tick++; },
		    [](void*) -> std::uint32_t { return 1000000; }};
	}
};

void softwareTests() {
	lexxos::platform::I2cPins pins{.index = 0, .sda = {21}, .scl = {22}, .baudRate = 100000};
	Software sw;
	lexxos::devices::SoftwareI2cBus bus{sw.io()};
	expect(bus.initialize(pins), "generic software backend accepts non-Lyra pins");
	sw.samples = {true, false, false, true}; // idle, address ACK, data ACK, STOP
	std::uint8_t byte = 0xA5;
	expect(bus.write(0x1F, &byte, 1) && sw.samples.empty(), "software write ACK");
	for (unsigned i = 0; i < 8; ++i) {
		expect(sw.clockData[i] == bool(0x3E & (0x80U >> i)), "software write address bits");
		expect(sw.clockData[9 + i] == bool(0xA5 & (0x80U >> i)), "software data bits");
	}

	sw.clockData.clear();
	sw.samples = {true, true};
	expect(bus.clearBus() && sw.samples.empty(), "idle bus clear must generate STOP without address bytes");
	expect(sw.clockData.size() == 1, "idle bus clear must not send nine clocks");
	sw.clockData.clear();
	sw.samples = {false, false, false, true, true};
	expect(bus.clearBus() && sw.samples.empty(), "interrupted byte releases SDA after bounded clock pulses");
	expect(sw.clockData.size() == 4, "three recovery pulses plus STOP expected");
	sw.stuckData = true;
	sw.clockData.clear();
	expect(!bus.clearBus() && sw.clockData.size() == 10, "stuck SDA must stop after nine pulses and STOP");
	expect(sw.sdaReleased && sw.sclReleased, "bus clear must release GPIO lines on failure");
	sw.stuckData = false;
	sw.stuckClock = true;
	auto clearStarted = sw.tick;
	expect(!bus.clearBus() && sw.tick - clearStarted < 20100, "bus clear clock stretch deadline");
	expect(sw.sdaReleased && sw.sclReleased, "stuck SCL cleanup");
	sw.stuckClock = false;
	sw.clockData.clear();
	sw.samples = {true, false};
	for (unsigned value : {0xA5U, 0x39U}) {
		for (unsigned i = 0; i < 8; ++i) {
			sw.samples.push_back(bool(value & (0x80U >> i)));
		}
	}

	sw.samples.push_back(true);
	std::uint8_t bytes[2]{};
	expect(bus.read(0x1F, bytes, 2) && bytes[0] == 0xA5 && bytes[1] == 0x39, "software read bits");
	expect(!sw.clockData[17] && sw.clockData[26], "software ACK then final NACK");
	sw.samples = {true, true, true};
	expect(!bus.probe(0x1F), "software address NACK");
	sw.samples = {true, false, true};
	sw.stretchReads = 5;
	expect(bus.probe(0x1F) && sw.stretchReads == 0, "finite clock stretching succeeds");
	sw.stuckClock = true;

	auto started = sw.tick;
	expect(!bus.probe(0x1F) && sw.tick - started < 20100, "clock stretching timeout");
	expect(sw.sdaReleased && sw.sclReleased, "release both pins after timeout");
	sw.stuckClock = false;
	sw.stuckData = true;
	sw.clockData.clear();
	expect(!bus.probe(0x1F) && sw.clockData.size() <= 12, "bounded nine-clock stuck-SDA recovery");
	sw.stuckData = false;
	sw.samples = {true, false, true};
	expect(bus.probe(0x1F), "software recovers after bus fault");
	pins.scl = pins.sda;
	expect(!bus.initialize(pins) && !bus.isReady() && sw.configured == 1, "invalid pins do not configure hardware");
}

} // namespace

int main() {
	hardwareTests();
	softwareTests();
}
