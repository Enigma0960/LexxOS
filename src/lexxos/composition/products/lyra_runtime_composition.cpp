#include <lexxos/composition/products/lyra_runtime_composition.h>

namespace lexxos::composition::products {

namespace {

	// Runtime adapters below still implement the fixed sd1 /mnt/sd service.
	// Reject valid but unimplemented declarations instead of silently using defaults.
	bool supportedProduct(hardware::products::LyraProduct const& product) {
		if (hardware::products::validateLyraProduct(product) != hardware::products::LyraProductError::None) {
			return false;
		}

		if (product.carrier == hardware::products::LyraCarrier::None) {
			return true;
		}

		auto const& slot = product.storage[1];
		return product.picoCalcWiring->storageBus.index == 1 && slot.hotSwappable && slot.mountPath != nullptr && std::string_view{slot.mountPath} == "/mnt/sd";
	}

} // namespace

LyraRuntimeResult registerLyraRuntimePrograms(hardware::products::LyraProduct const& product,
    bool diagnosticsEnabled, LyraRuntimeProviders const& providers) {
	if (!supportedProduct(product)) {
		return LyraRuntimeResult::InvalidProduct;
	}

	bool const removable = product.carrier == hardware::products::LyraCarrier::PicoCalc;
	if (!providers.systemStorage || !providers.debug || (removable && (!providers.configureStorage || !providers.removableStorage || (diagnosticsEnabled && !providers.removableDiagnostics)))) {
		return LyraRuntimeResult::MissingProvider;
	}

	if (removable && !providers.configureStorage(providers.context, product.storage)) {
		return LyraRuntimeResult::StorageConfigurationFailed;
	}

	providers.systemStorage(providers.context);
	if (removable) {
		providers.removableStorage(providers.context);
	}

	providers.debug(providers.context);
	if (removable && diagnosticsEnabled) {
		providers.removableDiagnostics(providers.context);
	}

	return LyraRuntimeResult::ProvidersInvoked;
}

LyraRuntimeResult startLyraRuntimeTasks(hardware::products::LyraProduct const& product,
    LyraRuntimeProviders const& providers) {
	if (!supportedProduct(product)) {
		return LyraRuntimeResult::InvalidProduct;
	}

	if (product.carrier == hardware::products::LyraCarrier::None) {
		return LyraRuntimeResult::NoTasks;
	}

	if (!providers.startHotplug || !providers.configureStorage) {
		return LyraRuntimeResult::MissingProvider;
	}

	if (!providers.configureStorage(providers.context, product.storage)) {
		return LyraRuntimeResult::StorageConfigurationFailed;
	}

	if (providers.startDisplay && !providers.startDisplay(providers.context)) {
		return LyraRuntimeResult::TaskCreationFailed;
	}

	return providers.startHotplug(providers.context)
	    ? LyraRuntimeResult::ProvidersInvoked
	    : LyraRuntimeResult::TaskCreationFailed;
}

} // namespace lexxos::composition::products
