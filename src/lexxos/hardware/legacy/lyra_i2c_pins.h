#pragma once

#include <lexxos/hardware/products/lyra_endpoints.h>
#include <lexxos/platform/board.h>

namespace lexxos::hardware::legacy {

// Only the legacy bus API requires flattened GPIO numbers.
[[nodiscard]] constexpr platform::I2cPins lyraI2cPins(products::LyraI2cEndpoint const& endpoint) {
	return {
	    .index = endpoint.route.id.index,
	    .sda = {endpoint.route.sda.bank * 32 + endpoint.route.sda.number},
	    .scl = {endpoint.route.scl.bank * 32 + endpoint.route.scl.number},
	    .baudRate = endpoint.frequencyHz,
	};
}

} // namespace lexxos::hardware::legacy
