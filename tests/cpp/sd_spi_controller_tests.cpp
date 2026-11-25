#include <array>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <map>
#include <vector>

#include <lexxos/composition/products/lyra_sd_spi.h>
#include <lexxos/devices/rk3506/sd_spi_controller.h>

namespace {

constexpr std::uintptr_t spi = 0xFF130000U;
std::map<std::uintptr_t, std::uint32_t> registers{
    {spi + 0x48U, 1U}
};
std::vector<std::pair<std::uintptr_t, std::uint32_t>> writes;
std::deque<std::uint8_t> received;
std::uint8_t nextByte{};
std::uint32_t ticks{};
bool stalled{}, rejectBaud{};

void expect(bool value, char const* message) {
	if (!value) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}

} // namespace

extern "C" std::uint32_t rk3506_timer_ticks() {
	return ++ticks;
}

extern "C" std::uint64_t rk3506_timer_counter() {
	return ticks;
}

extern "C" std::uint32_t sd_spi_test_read(std::uintptr_t address) {
	if (address == spi + 0x20U) {
		return static_cast<std::uint32_t>(received.size());
	}

	if (address == spi + 0x800U) {
		expect(!received.empty(), "read outside received frames");
		auto value = received.front();
		received.pop_front();
		return value;
	}

	return registers[address];
}

extern "C" void sd_spi_test_write(std::uintptr_t address, std::uint32_t value) {
	writes.emplace_back(address, value);
	if (address == spi + 0x400U && !stalled) {
		received.push_back(nextByte++);
	}

	if (address == spi + 8U && value == 0) {
		received.clear();
	}

	if (address == spi + 0x10U && rejectBaud) {
		rejectBaud = false;
		return;
	}

	if (address == 0xFF9A0388U) {
		auto const mask = value >> 16U;
		registers[address] = (registers[address] & ~mask) | (value & mask);
	} else {
		registers[address] = value;
	}
}

namespace lexxos::platform::rk3506 {

PicoSpi1DmaInfo picoSdDmaInfo() {
	return {};
}

} // namespace lexxos::platform::rk3506

int main() {
	namespace sd = lexxos::platform::rk3506;
	auto& owner = lexxos::composition::products::lyraSdSpi();
	sd::SdSpiController untouched;
	expect(writes.empty() && owner.version() == 0 && untouched.version() == 0, "construction touched MMIO");
	registers[0xFF9A0388U] = 0x3F0U;
	expect(sd::picoSpi1Initialize() && owner.divider() == 64, "startup divider changed");
	expect(registers[0xFF9A0388U] == 0x3F0U, "SPI1 changed SPI0 clock fields");
	for (auto const& entry : writes) {
		expect(entry.first < 0xFF480000U || entry.first >= 0xFF490000U, "boot SDMMC touched");
		expect(entry.first != 0xFF9100F8U, "GPIO CS remuxed");
	}

	expect(untouched.version() == 0, "initialization leaked into another instance");
	std::uint8_t byte{};
	nextByte = 0x40;
	expect(sd::picoSpi1Transfer(0xFF, byte) && byte == 0x40, "full duplex byte changed");
	for (auto mode : {sd::PicoSpi1FifoMode::Safe, sd::PicoSpi1FifoMode::Fast}) {
		expect(owner.setFifoMode(mode) && sd::picoSpi1FifoMode() == mode, "alias lost mode");
		expect(owner.resetFifoStatistics(), "stats reset failed");
		nextByte = 0;
		std::array<std::uint8_t, 65> data{};
		expect(sd::picoSpi1ReadData(data.data(), data.size()), "FIFO read failed");
		for (unsigned i = 0; i < data.size(); ++i) {
			expect(data[i] == i, "FIFO byte lost/reordered");
		}

		auto stats = owner.fifoStatistics();
		expect(stats.bytes == 65 && stats.reads == 1 && stats.peakPending <= 24 && stats.peakRxFifo <= 24, "FIFO bounds/stats changed");
		expect(untouched.fifoStatistics().reads == 0 && untouched.fifoMode() == sd::PicoSpi1FifoMode::Safe, "instance state leaked");
	}
	owner.fastClock();
	expect(sd::picoSpi1ClockMHz() == 6, "fast clock changed");
	rejectBaud = true;
	expect(!owner.setClockMHz(12) && owner.clockMHz() == 6, "failed clock change not restored");
	expect(owner.setClockMHz(12) && owner.clockMHz() == 12, "12 MHz rejected");
	writes.clear();
	registers[spi + 0x24U] = 1;
	expect(!owner.setClockMHz(6) && writes.empty(), "busy clock change touched MMIO");
	registers[spi + 0x24U] = 0;
	expect(!owner.setFifoMode(static_cast<sd::PicoSpi1FifoMode>(255)), "invalid mode accepted");
	std::array<std::uint8_t, 4> data{};
	received.push_back(1);
	expect(!owner.readData(data.data(), data.size()) && owner.fifoStatistics().syncErrors == 1 && owner.version() == 0, "stale RX did not abort");
	expect(owner.initialize() && owner.fifoMode() == sd::PicoSpi1FifoMode::Safe && owner.fifoStatistics().reads == 0, "reprobe did not reset state");
	registers[spi + 0x34U] = 8;
	expect(!owner.readData(data.data(), data.size()) && owner.fifoStatistics().overflows == 1, "RX overflow ignored");
	registers[spi + 0x34U] = 0;
	expect(owner.initialize(), "reinitialize failed");
	stalled = true;
	expect(!owner.readData(data.data(), data.size()) && owner.fifoStatistics().timeouts == 1, "read timeout ignored");
	expect(registers[spi + 8U] == 0 && owner.version() == 0, "abort left SPI enabled");
	expect(owner.initialize(), "reinitialize after timeout failed");
	owner.stop();
	writes.clear();
	expect(!owner.transfer(0xFF, byte) && !owner.readData(data.data(), 4) && writes.empty(), "stopped controller accessed hardware");
}
