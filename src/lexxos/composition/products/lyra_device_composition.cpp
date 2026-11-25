#include <lexxos/composition/products/lyra_device_composition.h>

namespace lexxos::composition::products {

LyraCompositionResult composeLyraDevices(
    hardware::products::LyraProduct const& product, bool displayEnabled,
    std::uint32_t displayHz, LyraDeviceProviders const& providers) {
	auto const endpoints = hardware::products::resolveLyraEndpoints(product);
	if (!endpoints) {
		return LyraCompositionResult::InvalidProduct;
	}

	if (product.carrier == hardware::products::LyraCarrier::None) {
		return LyraCompositionResult::NoDevices;
	}

	if (!providers.i2c || !providers.input || (displayEnabled && !providers.display)) {
		return LyraCompositionResult::MissingProvider;
	}

	auto* bus = providers.i2c(providers.context, endpoints->southbridge->route.id);
	if (!bus) {
		return LyraCompositionResult::MissingBus;
	}

	providers.input(providers.context, *bus);
	if (displayEnabled) {
		providers.display(providers.context, *endpoints->display, displayHz);
	}

	return LyraCompositionResult::ProvidersInvoked;
}

} // namespace lexxos::composition::products
