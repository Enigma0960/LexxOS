#include <array>
#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>

#include <lexxos/composition/products/lyra_lcd_spi.h>
#include <lexxos/devices/rk3506/lcd_spi_controller.h>

namespace {

constexpr std::uintptr_t spi = 0xFF120000U;
std::map<std::uintptr_t, std::uint32_t> registers{
    {spi + 0x48U, 1U}
};
std::vector<std::pair<std::uintptr_t, std::uint32_t>> writes;

class FakeDma final : public lexxos::platform::rk3506::LcdSpiDma {
public:
	bool retained{}, active{}, ok{true};
	unsigned byteWrites{}, wordWrites{};

	bool sourceRetained() const override { return retained; }

	bool enabled() override { return active; }

	bool write(std::uint8_t const*, std::size_t) override {
		++byteWrites;
		return ok;
	}

	bool write16(std::uint16_t const*, std::size_t) override {
		++wordWrites;
		return ok;
	}
};

FakeDma primary;
std::uint32_t ticks{};

void expect(bool value, char const* message) {
	if (!value) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}

} // namespace

extern "C" std::uint32_t rk3506_timer_ticks() {
	return ticks += 100;
}

extern "C" std::uint64_t rk3506_timer_counter() {
	return ++ticks;
}

extern "C" std::uint32_t rk3506_timer_counter_frequency() {
	return 24000000U;
}

extern "C" std::uint32_t lcd_spi_test_read(std::uintptr_t address) {
	return registers[address];
}

extern "C" void lcd_spi_test_write(std::uintptr_t address, std::uint32_t value) {
	writes.emplace_back(address, value);
	if (address == 0xFF9A0388U) {
		auto const mask = value >> 16U;
		registers[address] = (registers[address] & ~mask) | (value & mask);
	} else {
		registers[address] = value;
	}
}

namespace lexxos::composition::products {

platform::rk3506::LcdSpiDma& lyraLcdSpiDma() {
	return primary;
}

} // namespace lexxos::composition::products

int main() {
	namespace lcd = lexxos::platform::rk3506;
	auto& controller = lexxos::composition::products::lyraLcdSpi();
	FakeDma secondary;
	lcd::LcdSpiController untouched{secondary};
	expect(writes.empty() && controller.clockHz() == 0 && untouched.clockHz() == 0, "construction touched hardware");
	expect(!lcd::lcdSpi0Initialize(1) && writes.empty(), "unsupported clock touched MMIO");
	registers[0xFF9A0388U] = 0xFC00U; // SPI1 fields must survive SPI0 clock changes.
	expect(lcd::lcdSpi0Initialize(12000000), "initialize failed");
	expect(controller.clockHz() == 12000000 && untouched.clockHz() == 0, "state not instance-owned");
	expect((registers[0xFF9A0388U] & 0xFC00U) == 0xFC00U, "SPI1 clock fields changed");
	std::array<std::uint8_t, 3> bytes{1, 2, 3};
	secondary.retained = true;
	expect(!untouched.initialize(12000000), "secondary ignored its retained dependency");
	expect(controller.clockHz() == 12000000, "secondary retention changed primary");
	writes.clear();
	expect(lcd::lcdSpi0Write(bytes.data(), bytes.size()), "PIO write failed");
	expect(writes.size() == 3 && writes[0] == std::pair<std::uintptr_t, std::uint32_t>{spi + 0x400U, 1U} && writes[2].second == 3, "PIO payload changed");
	expect(controller.statistics().completedBytes == 3 && untouched.statistics().calls == 0, "statistics leaked across instances");

	// A retained DMA source forbids all mutation, even reinitialize/clock changes.
	primary.retained = true;
	writes.clear();
	std::array<std::uint16_t, 2> words{0x1234, 0x5678};
	expect(!lcd::lcdSpi0Initialize(6000000) && !lcd::lcdSpi0SetClockHz(6000000) && !lcd::lcdSpi0SetRgb565Direct16(true) && !lcd::lcdSpi0Write(bytes.data(), 3) && !lcd::lcdSpi0WriteRgb565Words(words.data(), 2), "retained DMA accepted mutation");
	expect(writes.empty() && controller.statistics().calls == 1, "retained source touched MMIO/stats");
	primary.retained = false;
	registers[spi + 0x24U] = 1;
	expect(!controller.setClockHz(6000000) && writes.empty(), "busy clock change accepted");
	registers[spi + 0x24U] = 0;
	expect(controller.setClockHz(6000000) && lcd::lcdSpi0ClockHz() == 6000000, "alias lost clock state");

	secondary.retained = false;
	expect(untouched.initialize(12000000), "independent controller init failed");
	primary.active = true;
	expect(!untouched.setRgb565Direct16(true), "secondary used primary DMA enable state");
	expect(lcd::lcdSpi0SetRgb565Direct16(true) && lcd::lcdSpi0Rgb565Direct16Enabled(), "direct16 enable failed");
	auto const dfs8 = registers[spi];
	expect(lcd::lcdSpi0WriteRgb565Words(words.data(), 2) && primary.wordWrites == 1, "direct16 not forwarded");
	expect(registers[spi] == dfs8 && registers[spi + 8] == 1, "DFS8 not restored after DMA");

	std::array<std::uint8_t, 512> block{};
	primary.ok = false;
	writes.clear();
	expect(!controller.write(block.data(), block.size()) && primary.byteWrites == 1, "DMA failure not returned");
	for (auto const& entry : writes) {
		expect(entry.first != spi + 0x400U, "partial DMA retried through PIO");
	}

	expect(lcd::lcdSpi0ClockHz() == 0 && registers[spi + 8] == 0, "failed controller still ready");
	primary.active = false;
	expect(controller.initialize(12000000), "reinitialize failed");
	registers[spi + 0x1CU] = 28;
	expect(!controller.write(bytes.data(), 3) && controller.clockHz() == 0, "FIFO timeout not bounded");
	registers[spi + 0x1CU] = 0;
	expect(controller.initialize(12000000), "reinitialize after timeout failed");
	registers[spi + 0x24U] = 1;
	expect(!controller.write(bytes.data(), 3) && controller.clockHz() == 0, "drain timeout ignored");
	expect(untouched.statistics().calls == 0 && untouched.clockHz() == 12000000, "failure leaked into other instance");
}
