#pragma once

#include <lexxos/hardware/products/lyra_endpoints.h>

namespace lexxos::composition::products {

// Runtime policy is separate from physical wiring. JCS retains the existing
// conservative clock; this does not identify or infer a hardware revision.
[[nodiscard]] constexpr hardware::products::LyraEndpoints applyLyraDevicePolicy(
    hardware::products::LyraEndpoints endpoints, bool jcsSouthbridge) {
	if (jcsSouthbridge && endpoints.southbridge) {
		endpoints.southbridge->frequencyHz = 10000U;
	}

	return endpoints;
}

// These are internal panel controls, not user GPIO. A declaration grants no
// access when the display is disabled or absent from the selected product.
[[nodiscard]] constexpr bool lyraDisplayOwnsGpio(hardware::products::LyraEndpoints const& endpoints,
    bool displayEnabled, hardware::modules::lyra::Pin pin) {
	if (!displayEnabled || !endpoints.display) {
		return false;
	}

	auto const& panel = *endpoints.display;
	return pin == panel.spi.cs || pin == panel.dc || pin == panel.reset;
}

} // namespace lexxos::composition::products
