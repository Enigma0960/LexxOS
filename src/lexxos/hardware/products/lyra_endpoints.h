#pragma once

#include <optional>

#include <lexxos/hardware/modules/picocalc.h>
#include <lexxos/hardware/products/lyra_product.h>

namespace lexxos::hardware::products {

struct LyraI2cEndpoint {
	modules::lyra::I2cInterface route;
	std::uint32_t frequencyHz;
};

struct LyraSpiEndpoint {
	modules::lyra::SpiInterface route;
	modules::lyra::Pin cs;
};

struct LyraDisplayEndpoint {
	LyraSpiEndpoint spi;
	modules::lyra::Pin dc;
	modules::lyra::Pin reset;
	int width{modules::picocalc::displayWidth};
	int height{modules::picocalc::displayHeight};
	bool invertColors{modules::picocalc::displayInvertColors};
};

// Value snapshot of physical routes. No SDK pins, LegacyBoardDescriptor, MMIO or
// borrowed product storage escapes resolution. Availability is not ownership.
struct LyraEndpoints {
	modules::lyra::UartInterface console;
	std::optional<LyraI2cEndpoint> southbridge;
	std::optional<LyraDisplayEndpoint> display;
	std::optional<LyraSpiEndpoint> removableStorage;
};

[[nodiscard]] constexpr std::optional<LyraEndpoints> resolveLyraEndpoints(LyraProduct const& product) {
	if (validateLyraProduct(product) != LyraProductError::None) {
		return std::nullopt;
	}

	LyraEndpoints result{*modules::lyra::findInterface(product.available.uarts, product.console), {}, {}, {}};
	if (product.carrier == LyraCarrier::PicoCalc) {
		auto const& wiring = *product.picoCalcWiring;
		result.southbridge = LyraI2cEndpoint{
		    *modules::lyra::findInterface(product.available.i2cs, wiring.southbridgeBus), wiring.southbridgeI2cHz};

		result.display = LyraDisplayEndpoint{
		    {*modules::lyra::findInterface(product.available.spis, wiring.displayBus), {0, *lyraGpio0Pin(wiring.displayCs)}},
		    {0, *lyraGpio0Pin(wiring.displayDc)},
		    {0, *lyraGpio0Pin(wiring.displayReset)}
        };

		result.removableStorage = LyraSpiEndpoint{
		    *modules::lyra::findInterface(product.available.spis, wiring.storageBus), wiring.storageCs};
	}

	return result;
}

} // namespace lexxos::hardware::products
