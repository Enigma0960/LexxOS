#include <cstdlib>
#include <iostream>
#include <utility>
#include <vector>

#include <lexxos/composition/products/lyra_device_policy.h>
#include <lexxos/hardware/products/lyra_products.h>
#include <lexxos/platform/rockchip/rk3506/gpio_output.h>

namespace {

std::vector<std::pair<std::uintptr_t, std::uint32_t>> writes;

void expect(bool value, char const* message) {
	if (!value) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}

} // namespace

extern "C" void rk3506_gpio_test_write(std::uintptr_t address, std::uint32_t value) {
	writes.emplace_back(address, value);
}

int main() {
	namespace hw = lexxos::hardware::products;
	namespace soc = lexxos::platform::rk3506;
	using lexxos::composition::products::lyraDisplayOwnsGpio;
	constexpr auto panel = *hw::resolveLyraEndpoints(hw::picoCalcLyraProduct);
	constexpr auto standalone = *hw::resolveLyraEndpoints(hw::lyraStandaloneProduct);

	for (int pin = -1; pin <= 64; ++pin) {
		expect(lyraDisplayOwnsGpio(panel, true, {0, pin}) == (pin >= 2 && pin <= 4), "wrong product ownership");
		expect(!lyraDisplayOwnsGpio(panel, false, {0, pin}), "disabled display owns GPIO");
		expect(!lyraDisplayOwnsGpio(standalone, true, {0, pin}), "standalone owns LCD GPIO");
		expect(!lyraDisplayOwnsGpio(panel, true, {1, pin}), "GPIO bank ignored");
	}

	for (unsigned pin : {2U, 3U, 4U}) {
		for (bool level : {false, true}) {
			writes.clear();
			expect(soc::configureGpio0Output(pin, level), "supported pin rejected");
			auto const bit = 1U << pin;
			std::vector<std::pair<std::uintptr_t, std::uint32_t>> const expected{
			    {                        0xFF9B0800U,                       0x01800000U},
			    {pin < 4 ? 0xFF950000U : 0xFF950004U,   0xFU << ((pin % 4U) * 4U + 16U)},
			    {                        0xFF940000U, (bit << 16U) | (level ? bit : 0U)},
			    {                        0xFF940008U,                (bit << 16U) | bit},
			};

			expect(writes == expected, "clock/mux/data/direction trace changed");
			writes.clear();
			expect(soc::writeGpio0Output(pin, level), "write rejected");
			expect(writes.size() == 1 && writes[0] == expected[2], "write changed other registers");
		}
	}

	for (unsigned pin : {0U, 1U, 5U, 31U, 32U, ~0U}) {
		writes.clear();
		expect(!soc::configureGpio0Output(pin, true) && !soc::writeGpio0Output(pin, true), "unsupported route accepted");
		expect(writes.empty(), "unsupported route touched MMIO");
	}
}
