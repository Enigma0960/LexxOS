#include <cstdlib>
#include <iostream>

#include <lexxos/composition/products/lyra_device_composition.h>
#include <lexxos/devices/bus/i2c_bus.h>
#include <lexxos/hardware/products/lyra_products.h>

namespace {

namespace composition = lexxos::composition::products;
namespace hardware = lexxos::hardware::products;
using composition::LyraCompositionResult;

// Not-ready bus deliberately exercises registration before hardware is present.
class FakeBus final : public lexxos::devices::I2cBus {
public:
	bool initialize(lexxos::platform::I2cPins const&) override { std::abort(); }

	bool isReady() const override { return false; }

	bool probe(std::uint8_t) override { std::abort(); }

	bool write(std::uint8_t, std::uint8_t const*, std::size_t) override { std::abort(); }
};

struct State {
	FakeBus bus;
	unsigned resolutions{}, inputs{}, displays{};
	int requested{-1};
	bool missingBus{false};
	lexxos::devices::I2cBus* injected{};
	hardware::LyraDisplayEndpoint endpoint{};
	std::uint32_t frequencyHz{};
};

void expect(bool condition, char const* message) {
	if (!condition) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}

lexxos::devices::I2cBus* resolve(void* opaque, lexxos::hardware::modules::lyra::I2cId id) {
	auto& state = *static_cast<State*>(opaque);
	++state.resolutions;
	state.requested = id.index;
	return state.missingBus ? nullptr : &state.bus;
}

void input(void* opaque, lexxos::devices::I2cBus& bus) {
	auto& state = *static_cast<State*>(opaque);
	++state.inputs;
	state.injected = &bus;
}

void display(void* opaque, hardware::LyraDisplayEndpoint const& endpoint, std::uint32_t frequencyHz) {
	auto& state = *static_cast<State*>(opaque);
	expect(state.inputs == 1, "display registered before input");
	++state.displays;
	state.endpoint = endpoint;
	state.frequencyHz = frequencyHz;
}

composition::LyraDeviceProviders providers(State& state) {
	return {&state, resolve, input, display};
}

} // namespace

int main() {
	{
		State state;
		auto result = composition::composeLyraDevices(hardware::picoCalcLyraProduct, true, 12000000, providers(state));
		expect(result == LyraCompositionResult::ProvidersInvoked, "PicoCalc rejected");
		expect(state.resolutions == 1 && state.requested == 2, "physical I2C2 not resolved exactly once");
		expect(state.inputs == 1 && state.injected == &state.bus && state.displays == 1, "providers did not receive owned bus");
		expect(state.endpoint.spi.route.id.index == 0 && state.frequencyHz == 12000000 && state.endpoint.width == 320,
		    "display binding/override changed");
	}
	{
		State state;
		auto hooks = providers(state);
		hooks.display = nullptr;
		expect(composition::composeLyraDevices(hardware::picoCalcLyraProduct, false, 0, hooks)
		        == LyraCompositionResult::ProvidersInvoked,
		    "display-off requires display provider");
		expect(state.inputs == 1 && state.displays == 0, "display-off changed input registration");
	}
	{
		State state;
		expect(composition::composeLyraDevices(hardware::lyraStandaloneProduct, true, 12000000, providers(state))
		        == LyraCompositionResult::NoDevices,
		    "standalone requires PicoCalc providers");
		expect(state.resolutions == 0 && state.inputs == 0 && state.displays == 0, "standalone touched peripheral");
		expect(composition::composeLyraDevices(hardware::lyraStandaloneProduct, true, 12000000, {})
		        == LyraCompositionResult::NoDevices,
		    "standalone rejected empty provider set");
	}
	{
		State state;
		auto product = hardware::picoCalcLyraProduct;
		product.console = {1};
		expect(composition::composeLyraDevices(product, true, 12000000, providers(state))
		        == LyraCompositionResult::InvalidProduct,
		    "conflicting product accepted");
		expect(state.resolutions == 0 && state.inputs == 0 && state.displays == 0, "invalid product had side effects");
	}
	{
		State state;
		auto hooks = providers(state);
		hooks.display = nullptr;
		expect(composition::composeLyraDevices(hardware::picoCalcLyraProduct, true, 12000000, hooks)
		        == LyraCompositionResult::MissingProvider,
		    "missing display ignored");
		expect(state.resolutions == 0 && state.inputs == 0, "partial registration before binding validation");
		hooks = providers(state);
		hooks.input = nullptr;
		expect(composition::composeLyraDevices(hardware::picoCalcLyraProduct, false, 0, hooks)
		        == LyraCompositionResult::MissingProvider,
		    "missing input ignored");
		hooks = providers(state);
		hooks.i2c = nullptr;
		expect(composition::composeLyraDevices(hardware::picoCalcLyraProduct, false, 0, hooks)
		        == LyraCompositionResult::MissingProvider,
		    "missing resolver ignored");
	}
	{
		State state;
		state.missingBus = true;
		expect(composition::composeLyraDevices(hardware::picoCalcLyraProduct, true, 12000000, providers(state))
		        == LyraCompositionResult::MissingBus,
		    "missing physical bus ignored");
		expect(state.inputs == 0 && state.displays == 0, "partial registration with missing bus");
	}
}
