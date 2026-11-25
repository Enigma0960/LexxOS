#include <cstdlib>
#include <iostream>
#include <utility>
#include <vector>

#include <lexxos/platform/rockchip/rk3506/sd_gpio.h>

namespace {

std::vector<std::pair<std::uintptr_t, std::uint32_t>> writes;
std::uint32_t gate{}, inputs{};
unsigned inputReads{};
bool serial{};

void expect(bool ok, char const* message) {
	if (!ok) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}

} // namespace

extern "C" std::uint32_t sd_gpio_test_read(std::uintptr_t address) {
	if (address == 0xFF9A080CU) {
		return gate;
	}

	if (address == 0xFF870070U) {
		++inputReads;
		return serial ? ((0xA5U >> (8U - inputReads)) & 1U) << 27U : inputs;
	}

	return 0;
}

extern "C" void sd_gpio_test_write(std::uintptr_t address, std::uint32_t value) {
	writes.emplace_back(address, value);
}

int main() {
	namespace gpio = lexxos::platform::rk3506;
	gpio::sdGpioConfigure();
	std::vector<std::pair<std::uintptr_t, std::uint32_t>> const expected{
	    {0xFF9A080CU, 0x01000000U},
	    {0xFF870004U, 0x06080408U},
	    {0xFF87000CU, 0x0E080608U},
	    {0xFF660030U, 0xF0000000U},
	    {0xFF660038U, 0xFFF00000U},
	};
	expect(writes == expected, "GPIO configure changed clock/data/direction/mux trace");
	writes.clear();
	gpio::sdGpioChipSelect(false);
	gpio::sdGpioChipSelect(true);
	expect(writes.size() == 2 && writes[0].second == 0x04000000U && writes[1].second == 0x04000400U, "CS changed unrelated outputs");
	writes.clear();
	serial = true;
	inputReads = 0;
	expect(gpio::sdGpioTransfer(0x96) == 0xA5 && inputReads == 8 && writes.size() == 24, "MSB-first transfer changed");
	for (unsigned i = 0; i < 8; ++i) {
		expect(writes[i * 3].first == 0xFF870004U
		        && writes[i * 3].second == (0x02080000U | (((0x96U >> (7U - i)) & 1U) ? 8U : 0U))
		        && writes[i * 3 + 1].second == 0x02000200U
		        && writes[i * 3 + 2].second == 0x02000000U,
		    "bit transfer changed masks/clock edges");
	}

	serial = false;
	inputReads = 0;
	writes.clear();
	gate = 1U << 8U;
	auto snapshot = gpio::sdGpioRegisters(false);
	expect(!snapshot.detectReadable && inputReads == 0 && writes.empty(), "diagnostics touched gated GPIO");
	inputs = 1U << 10U;
	expect(gpio::sdGpioRegisters(true).detectHigh, "configured detect not read");
	gate = 0;
	expect(gpio::sdGpioRegisters(false).detectReadable && !gpio::sdGpioCardInserted(), "detect polarity changed");
	inputs = 0;
	expect(gpio::sdGpioCardInserted(), "inserted card not detected");
	writes.clear();
	gpio::sdGpioEnableClock();
	expect(writes.size() == 1 && writes[0] == expected[0], "hotplug changed other clocks");
}
