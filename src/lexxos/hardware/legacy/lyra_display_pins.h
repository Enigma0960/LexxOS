#pragma once

#include <lexxos/hardware/products/lyra_endpoints.h>
#include <lexxos/platform/board.h>

namespace lexxos::hardware::legacy {

// Compatibility projection for the legacy backend/board only. Caller supplies
// an endpoint resolved from a validated product and the stage's clock policy.
[[nodiscard]] constexpr platform::SpiTftDisplayPins lyraDisplayPins(
    products::LyraDisplayEndpoint const& endpoint, std::uint32_t frequencyHz) {
	auto const number = [](modules::lyra::Pin pin) {
		return pin.bank * 32 + pin.number;
	};

	return {
	    .enabled = true,
	    .spiIndex = endpoint.spi.route.id.index,
	    .spiBaudRate = frequencyHz,
	    .sck = {number(endpoint.spi.route.clock)},
	    .mosi = {number(endpoint.spi.route.mosi)},
	    .cs = {number(endpoint.spi.cs)},
	    .dc = {number(endpoint.dc)},
	    .reset = {number(endpoint.reset)},
	    .width = endpoint.width,
	    .height = endpoint.height,
	    .invertColors = endpoint.invertColors,
	};
}

} // namespace lexxos::hardware::legacy
